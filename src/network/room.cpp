// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <mutex>
#include <random>
#include <regex>
#include <sstream>
#include <thread>
#include <enet/enet.h>
#include <httplib.h>
#include <json.hpp>
#include "common/logging/log.h"
#include "common/thread_pool.h"
#include "network/packet.h"
#include "network/room.h"

namespace Network {

void to_json(nlohmann::json& json, const JSONRoom::Member& member) {
    json["name"] = member.name;
    json["program"] = member.program;
}

void from_json(const nlohmann::json& json, JSONRoom::Member& member) {
    member.name = json.at("name").get<std::string>();
    member.program = json.at("program").get<std::string>();
}

void to_json(nlohmann::json& json, const JSONRoom& room) {
    json["name"] = room.name;
    json["creator"] = room.creator;
    if (!room.description.empty())
        json["description"] = room.description;
    json["port"] = room.port;
    json["maxMembers"] = room.max_members;
    json["netVersion"] = room.net_version;
    json["hasPassword"] = room.has_password;
    if (room.members.size() > 0) {
        nlohmann::json member_json = room.members;
        json["members"] = member_json;
    }
}

void from_json(const nlohmann::json& json, JSONRoom& room) {
    room.ip = json.at("ip").get<std::string>();
    room.name = json.at("name").get<std::string>();
    room.creator = json.at("creator").get<std::string>();
    try {
        room.description = json.at("description").get<std::string>();
    } catch (const nlohmann::detail::out_of_range& e) {
        room.description = "";
        LOG_DEBUG(Network, "Room '{}' doesn't contain a description", room.name);
    }
    room.port = json.at("port").get<u16>();
    room.max_members = json.at("maxMembers").get<u32>();
    room.net_version = json.at("netVersion").get<u32>();
    room.has_password = json.at("hasPassword").get<bool>();
    try {
        room.members = json.at("members").get<std::vector<JSONRoom::Member>>();
    } catch (const nlohmann::detail::out_of_range& e) {
        LOG_DEBUG(Network, "Out of range {}", e.what());
    }
}

struct Room::RoomImpl {
    RoomImpl()
        : random_gen{std::random_device{}()}, client{std::make_unique<httplib::Client>(
                                                  "citra-valentin-api.us.openode.io", 80)} {
        http_server.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
            res.status = 200;
            res.body = "OK";
        });
    }

    ErrorCallback error_callback;

    std::unique_ptr<httplib::Client> client;
    httplib::Server http_server;

    std::mt19937 random_gen; ///< Random number generator. Used for GenerateMACAddress

    ENetHost* server; ///< Network interface.

    std::atomic_bool is_open{}, is_public{};
    RoomInformation room_information; ///< Information about this room.

    std::string password; ///< The password required to connect to this room.

    struct Member {
        std::string nickname; ///< The nickname of the member.
        u64 console_id;
        std::string program;    ///< The current program of the member.
        MACAddress mac_address; ///< The assigned MAC address of the member.
        ENetPeer* peer;         ///< The remote peer.
    };

    using MemberList = std::vector<Member>;
    MemberList members;              ///< Information about the members of this room
    mutable std::mutex member_mutex; ///< Mutex for locking the members list

    BanList ban_list;                  ///< List of banned IP addresses
    mutable std::mutex ban_list_mutex; ///< Mutex for locking the ban list

    std::unique_ptr<std::thread>
        room_thread; ///< Thread that receives and dispatches network packets

    void ServerLoop(); ///< Thread function that will receive and dispatch messages until the room
                       ///< is destroyed.
    void StartLoop();

    /**
     * Parses and answers a room join request from a client.
     * Validates the uniqueness of the nicknamename and assigns the MAC address
     * that the client will use for the remainder of the connection.
     */
    void HandleJoinRequest(const ENetEvent* event);

    /**
     * Parses and answers a kick request from a client.
     * Validates the permissions and that the given user exists and then kicks the member.
     */
    void HandleModKickPacket(const ENetEvent* event);

    /**
     * Parses and answers a ban request from a client.
     * Validates the permissions and bans the user by IP.
     */
    void HandleModBanPacket(const ENetEvent* event);

    /**
     * Parses and answers a unban request from a client.
     * Validates the permissions and unbans the address.
     */
    void HandleModUnbanPacket(const ENetEvent* event);

    /**
     * Parses and answers a get ban list request from a client.
     * Validates the permissions and returns the ban list.
     */
    void HandleModGetBanListPacket(const ENetEvent* event);

    /// Returns whether the nickname is valid, ie. isn't already taken by someone else
    /// in the room.
    bool IsValidNickname(const std::string& nickname) const;

    /// Returns whether the MAC address is valid, ie. isn't already taken by someone else in the
    /// room.
    bool IsValidMACAddress(const MACAddress& address) const;

    /**
     * Returns whether the console ID is valid, ie. isn't already taken by someone else in
     * the room.
     */
    bool IsValidConsoleID(u64 console_id) const;

    /// Returns whether a nickname has mod permissions.
    bool HasModPermission(const ENetPeer* client) const;

    /// Sends a IDRoomIsFull message telling the client that the room is full.
    void SendRoomIsFull(ENetPeer* client);

    /// Sends a IDNameCollision message telling the client that the name is invalid.
    void SendNameCollision(ENetPeer* client);

    /// Sends a IDMACCollision message telling the client that the MAC is invalid.
    void SendMacCollision(ENetPeer* client);

    /**
     * Sends a IDConsoleIDCollision message telling the client that another member with the same
     * console ID exists.
     */
    void SendConsoleIDCollision(ENetPeer* client);

    /// Sends a IDVersionMismatch message telling the client that the version is invalid.
    void SendVersionMismatch(ENetPeer* client);

    /// Sends a IDWrongPassword message telling the client that the password is wrong.
    void SendWrongPassword(ENetPeer* client);

    /**
     * Notifies the member that its connection attempt was successful,
     * and it is now part of the room.
     */
    void SendJoinSuccess(ENetPeer* client, MACAddress mac_address);

    /// Sends a IDHostKicked message telling the client that they have been kicked.
    void SendUserKicked(ENetPeer* client);

    /// Sends a IDHostBanned message telling the client that they have been banned.
    void SendUserBanned(ENetPeer* client);

    /**
     * Sends a IDModPermissionDenied message telling the client that they don't have mod
     * permission.
     */
    void SendModPermissionDenied(ENetPeer* client);

    /// Sends a IDModNoSuchUser message telling the client that the given user couldn't be found.
    void SendModNoSuchUser(ENetPeer* client);

    /// Sends the ban list in response to a client's request for getting ban list.
    void SendModBanListResponse(ENetPeer* client);

    /// Notifies the members that the room is closed.
    void SendCloseMessage();

    /// Sends a system message to all the connected clients.
    void SendStatusMessage(StatusMessageTypes type, const std::string& nickname);

    /**
     * Sends the information about the room, along with the list of members
     * to every connected client in the room.
     * The packet has the structure:
     * <MessageID> ID_ROOM_INFORMATION
     * <String> room_name
     * <String> room_description
     * <u32> max_members: The max number of clients allowed in this room
     * <u16> port
     * <u32> num_members: the number of currently joined clients
     * This is followed by the following three values for each member:
     * <String> nickname of that member
     * <MACAddress> mac_address of that member
     * <String> program of that member
     */
    void BroadcastRoomInformation();

    /// Generates a free MAC address to assign to a new client.
    MACAddress GenerateMACAddress();

    /**
     * Broadcasts this packet to all members except the sender.
     * @param event The ENet event containing the data
     */
    void HandleWiFiPacket(const ENetEvent* event);

    /**
     * Extracts a chat entry from a received ENet packet and adds it to the chat queue.
     * @param event The ENet event that was received.
     */
    void HandleChatPacket(const ENetEvent* event);

    /**
     * Extracts the program information from a received ENet packet and broadcasts it.
     * @param event The ENet event that was received.
     */
    void HandleProgramPacket(const ENetEvent* event);

    /**
     * Removes the client from the members list if it was in it and announces the change
     * to all other clients.
     */
    void HandleClientDisconnection(ENetPeer* client);

    Common::WebResult MakeRequest(const std::string& method, const std::string& body = "");
    std::vector<JSONRoom> GetRoomList();
    void UpdateAPIInformation();

    void SetErrorCallback(ErrorCallback cb) {
        error_callback = cb;
    }
};

// RoomImpl
void Room::RoomImpl::ServerLoop() {
    while (is_open.load(std::memory_order_relaxed)) {
        ENetEvent event;
        if (enet_host_service(server, &event, 50) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                switch (event.packet->data[0]) {
                case IDJoinRequest:
                    HandleJoinRequest(&event);
                    break;
                case IDSetProgram:
                    HandleProgramPacket(&event);
                    break;
                case IDWiFiPacket:
                    HandleWiFiPacket(&event);
                    break;
                case IDChatMessage:
                    HandleChatPacket(&event);
                    break;
                // Moderation
                case IDModKick:
                    HandleModKickPacket(&event);
                    break;
                case IDModBan:
                    HandleModBanPacket(&event);
                    break;
                case IDModUnban:
                    HandleModUnbanPacket(&event);
                    break;
                case IDModGetBanList:
                    HandleModGetBanListPacket(&event);
                    break;
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                HandleClientDisconnection(event.peer);
                break;
            case ENET_EVENT_TYPE_NONE:
            case ENET_EVENT_TYPE_CONNECT:
                break;
            }
        }
    }
    // Close the connection to all members:
    SendCloseMessage();
}

void Room::RoomImpl::StartLoop() {
    room_thread = std::make_unique<std::thread>(&Room::RoomImpl::ServerLoop, this);
}

void Room::RoomImpl::HandleJoinRequest(const ENetEvent* event) {
    {
        std::lock_guard lock{member_mutex};
        if (members.size() >= room_information.max_members) {
            SendRoomIsFull(event->peer);
            return;
        }
    }
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string nickname;
    packet >> nickname;
    u64 console_id;
    packet >> console_id;
    MACAddress preferred_mac;
    packet >> preferred_mac;
    u32 client_version;
    packet >> client_version;
    std::string pass;
    packet >> pass;
    if (pass != password) {
        SendWrongPassword(event->peer);
        return;
    }
    if (!IsValidNickname(nickname)) {
        SendNameCollision(event->peer);
        return;
    }
    if (preferred_mac != BroadcastMac) {
        // Verify if the preferred MAC address is available
        if (!IsValidMACAddress(preferred_mac)) {
            SendMacCollision(event->peer);
            return;
        }
    } else
        // Assign a MAC address of this client automatically
        preferred_mac = GenerateMACAddress();
    if (!IsValidConsoleID(console_id)) {
        SendConsoleIDCollision(event->peer);
        return;
    }
    if (client_version != NetworkVersion) {
        SendVersionMismatch(event->peer);
        return;
    }
    // At this point the client is ready to be added to the room.
    Member member;
    member.mac_address = preferred_mac;
    member.console_id = console_id;
    member.nickname = nickname;
    member.peer = event->peer;
    {
        std::lock_guard lock{ban_list_mutex};
        // Check IP ban
        char ip_raw[256];
        enet_address_get_host_ip(&event->peer->address, ip_raw, sizeof(ip_raw) - 1);
        std::string ip{ip_raw};
        if (std::find(ban_list.begin(), ban_list.end(), ip) != ban_list.end()) {
            SendUserBanned(event->peer);
            return;
        }
    }
    // Notify everyone that the user has joined.
    SendStatusMessage(IDMemberJoined, member.nickname);
    {
        std::lock_guard lock{member_mutex};
        members.push_back(std::move(member));
    }
    // Notify everyone that the room information has changed.
    BroadcastRoomInformation();
    SendJoinSuccess(event->peer, preferred_mac);
}

void Room::RoomImpl::HandleModKickPacket(const ENetEvent* event) {
    if (!HasModPermission(event->peer)) {
        SendModPermissionDenied(event->peer);
        return;
    }
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string nickname;
    packet >> nickname;
    {
        std::lock_guard lock{member_mutex};
        const auto target_member{
            std::find_if(members.begin(), members.end(),
                         [&nickname](const auto& member) { return member.nickname == nickname; })};
        if (target_member == members.end()) {
            SendModNoSuchUser(event->peer);
            return;
        }
        // Notify the kicked member
        SendUserKicked(target_member->peer);
        enet_peer_disconnect(target_member->peer, 0);
        members.erase(target_member);
    }
    // Announce the change to all clients.
    SendStatusMessage(IDMemberKicked, nickname);
    BroadcastRoomInformation();
}

void Room::RoomImpl::HandleModBanPacket(const ENetEvent* event) {
    if (!HasModPermission(event->peer)) {
        SendModPermissionDenied(event->peer);
        return;
    }
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string nickname;
    packet >> nickname;
    std::string ip;
    {
        std::lock_guard lock{member_mutex};
        const auto target_member{
            std::find_if(members.begin(), members.end(),
                         [&nickname](const auto& member) { return member.nickname == nickname; })};
        if (target_member == members.end()) {
            SendModNoSuchUser(event->peer);
            return;
        }
        // Notify the banned member
        SendUserBanned(target_member->peer);
        nickname = target_member->nickname;
        char ip_raw[256];
        enet_address_get_host_ip(&target_member->peer->address, ip_raw, 256);
        ip = ip_raw;
        enet_peer_disconnect(target_member->peer, 0);
        members.erase(target_member);
    }
    {
        std::lock_guard lock{ban_list_mutex};
        // Ban the member's IP
        if (std::find(ban_list.begin(), ban_list.end(), ip) == ban_list.end())
            ban_list.push_back(ip);
    }
    // Announce the change to all clients.
    SendStatusMessage(IDMemberBanned, nickname);
    BroadcastRoomInformation();
}

void Room::RoomImpl::HandleModUnbanPacket(const ENetEvent* event) {
    if (!HasModPermission(event->peer)) {
        SendModPermissionDenied(event->peer);
        return;
    }
    Packet packet;
    packet.Append(event->packet->data, event->packet->dataLength);
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string address;
    packet >> address;
    bool unbanned{};
    {
        std::lock_guard lock{ban_list_mutex};
        auto it = std::find(ban_list.begin(), ban_list.end(), address);
        if (it != ban_list.end()) {
            unbanned = true;
            ban_list.erase(it);
        }
    }
    if (unbanned)
        SendStatusMessage(IDAddressUnbanned, address);
    else
        SendModNoSuchUser(event->peer);
}

void Room::RoomImpl::HandleModGetBanListPacket(const ENetEvent* event) {
    if (!HasModPermission(event->peer)) {
        SendModPermissionDenied(event->peer);
        return;
    }
    SendModBanListResponse(event->peer);
}

bool Room::RoomImpl::IsValidNickname(const std::string& nickname) const {
    // A nickname is valid if it matches the regex and isn't already taken by anybody else in the
    // room.
    const std::regex nickname_regex{"^[ a-zA-Z0-9._-]{4,20}$"};
    if (!std::regex_match(nickname, nickname_regex))
        return false;
    std::lock_guard lock{member_mutex};
    return std::all_of(members.begin(), members.end(),
                       [&nickname](const auto& member) { return member.nickname != nickname; });
}

bool Room::RoomImpl::IsValidMACAddress(const MACAddress& address) const {
    // A MAC address is valid if it isn't already taken by anybody else in the room.
    std::lock_guard lock{member_mutex};
    return std::all_of(members.begin(), members.end(),
                       [&address](const auto& member) { return member.mac_address != address; });
}

bool Room::RoomImpl::IsValidConsoleID(u64 console_id) const {
    // A console ID is valid if it isn't already taken by anybody else in the room.
    std::lock_guard lock{member_mutex};
    return std::all_of(members.begin(), members.end(), [&console_id](const auto& member) {
        return member.console_id != console_id;
    });
}

bool Room::RoomImpl::HasModPermission(const ENetPeer* client) const {
    if (room_information.creator.empty())
        return false; // This room doesn't support moderation
    std::lock_guard lock{member_mutex};
    const auto sending_member{
        std::find_if(members.begin(), members.end(),
                     [client](const auto& member) { return member.peer == client; })};
    if (sending_member == members.end())
        return false;
    return sending_member->nickname == room_information.creator;
}

void Room::RoomImpl::SendNameCollision(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDNameCollision);
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendMacCollision(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDMACCollision);
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendConsoleIDCollision(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDConsoleIDCollision);
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendWrongPassword(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDWrongPassword);
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendRoomIsFull(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDRoomIsFull);
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendVersionMismatch(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDVersionMismatch);
    packet << NetworkVersion;
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendJoinSuccess(ENetPeer* client, MACAddress mac_address) {
    Packet packet;
    packet << static_cast<u8>(IDJoinSuccess);
    packet << mac_address;
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendUserKicked(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDHostKicked);
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendUserBanned(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDHostBanned);
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendModPermissionDenied(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDModPermissionDenied);
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendModNoSuchUser(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDModNoSuchUser);
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendModBanListResponse(ENetPeer* client) {
    Packet packet;
    packet << static_cast<u8>(IDModBanListResponse);
    {
        std::lock_guard lock{ban_list_mutex};
        packet << ban_list;
    }
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_peer_send(client, 0, enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::SendCloseMessage() {
    Packet packet;
    packet << static_cast<u8>(IDCloseRoom);
    std::lock_guard lock{member_mutex};
    if (!members.empty()) {
        auto enet_packet{
            enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
        for (auto& member : members)
            enet_peer_send(member.peer, 0, enet_packet);
    }
    enet_host_flush(server);
    for (auto& member : members)
        enet_peer_disconnect(member.peer, 0);
}

void Room::RoomImpl::SendStatusMessage(StatusMessageTypes type, const std::string& nickname) {
    Packet packet;
    packet << static_cast<u8>(IDStatusMessage);
    packet << static_cast<u8>(type);
    packet << nickname;
    std::lock_guard lock{member_mutex};
    if (!members.empty()) {
        auto enet_packet{
            enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
        for (auto& member : members)
            enet_peer_send(member.peer, 0, enet_packet);
    }
    enet_host_flush(server);
}

void Room::RoomImpl::BroadcastRoomInformation() {
    Packet packet;
    packet << static_cast<u8>(IDRoomInformation);
    packet << room_information.name;
    packet << room_information.description;
    packet << room_information.max_members;
    packet << room_information.port;
    packet << room_information.creator;
    packet << static_cast<u32>(members.size());
    {
        std::lock_guard lock{member_mutex};
        for (const auto& member : members) {
            packet << member.nickname;
            packet << member.mac_address;
            packet << member.program;
        }
    }
    auto enet_packet{
        enet_packet_create(packet.GetData(), packet.GetDataSize(), ENET_PACKET_FLAG_RELIABLE)};
    enet_host_broadcast(server, 0, enet_packet);
    enet_host_flush(server);
    if (is_public.load(std::memory_order_relaxed)) {
        // Update API information
        auto& thread_pool{Common::ThreadPool::GetPool()};
        thread_pool.Push([this] { UpdateAPIInformation(); });
    }
}

MACAddress Room::RoomImpl::GenerateMACAddress() {
    // The first three bytes of each MAC address will be the NintendoOUI
    MACAddress result_mac{NintendoOUI};
    std::uniform_int_distribution<> dis{0x00, 0xFF}; // Random byte between 0 and 0xFF
    do {
        for (std::size_t i{3}; i < result_mac.size(); ++i)
            result_mac[i] = dis(random_gen);
    } while (!IsValidMACAddress(result_mac));
    return result_mac;
}

void Room::RoomImpl::HandleWiFiPacket(const ENetEvent* event) {
    Packet in_packet;
    in_packet.Append(event->packet->data, event->packet->dataLength);
    in_packet.IgnoreBytes(sizeof(u8));         // Message type
    in_packet.IgnoreBytes(sizeof(u8));         // WiFiPacket Type
    in_packet.IgnoreBytes(sizeof(u8));         // WiFiPacket Channel
    in_packet.IgnoreBytes(sizeof(MACAddress)); // WiFiPacket Transmitter Address
    MACAddress destination_address;
    in_packet >> destination_address;
    Packet out_packet;
    out_packet.Append(event->packet->data, event->packet->dataLength);
    auto enet_packet{enet_packet_create(out_packet.GetData(), out_packet.GetDataSize(),
                                        ENET_PACKET_FLAG_RELIABLE)};
    if (destination_address == BroadcastMac) { // Send the data to everyone except the sender
        std::lock_guard lock{member_mutex};
        bool sent_packet{};
        for (const auto& member : members) {
            if (member.peer != event->peer) {
                sent_packet = true;
                enet_peer_send(member.peer, 0, enet_packet);
            }
        }
        if (!sent_packet)
            enet_packet_destroy(enet_packet);
    } else { // Send the data only to the destination client
        std::lock_guard lock{member_mutex};
        auto member{std::find_if(members.begin(), members.end(),
                                 [destination_address](const Member& member) -> bool {
                                     return member.mac_address == destination_address;
                                 })};
        if (member != members.end())
            enet_peer_send(member->peer, 0, enet_packet);
        else {
            LOG_ERROR(Network,
                      "Attempting to send to unknown MAC address: "
                      "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                      destination_address[0], destination_address[1], destination_address[2],
                      destination_address[3], destination_address[4], destination_address[5]);
            enet_packet_destroy(enet_packet);
        }
    }
    enet_host_flush(server);
}

void Room::RoomImpl::HandleChatPacket(const ENetEvent* event) {
    Packet in_packet;
    in_packet.Append(event->packet->data, event->packet->dataLength);
    in_packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string message;
    in_packet >> message;
    auto CompareNetworkAddress{
        [event](const Member member) -> bool { return member.peer == event->peer; }};
    std::lock_guard lock{member_mutex};
    const auto sending_member{std::find_if(members.begin(), members.end(), CompareNetworkAddress)};
    if (sending_member == members.end())
        return; // Received a chat message from a unknown sender
    // Limit the size of chat messages to MaxMessageSize
    message.resize(MaxMessageSize);
    Packet out_packet;
    out_packet << static_cast<u8>(IDChatMessage);
    out_packet << sending_member->nickname;
    out_packet << message;
    auto enet_packet{enet_packet_create(out_packet.GetData(), out_packet.GetDataSize(),
                                        ENET_PACKET_FLAG_RELIABLE)};
    bool sent_packet{};
    for (const auto& member : members) {
        if (member.peer != event->peer) {
            sent_packet = true;
            enet_peer_send(member.peer, 0, enet_packet);
        }
    }
    if (!sent_packet)
        enet_packet_destroy(enet_packet);
    enet_host_flush(server);
}

void Room::RoomImpl::HandleProgramPacket(const ENetEvent* event) {
    Packet in_packet;
    in_packet.Append(event->packet->data, event->packet->dataLength);
    in_packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string program;
    in_packet >> program;
    {
        std::lock_guard lock{member_mutex};
        auto member{
            std::find_if(members.begin(), members.end(), [event](const Member& member) -> bool {
                return member.peer == event->peer;
            })};
        if (member != members.end())
            member->program = program;
    }
    BroadcastRoomInformation();
}

void Room::RoomImpl::HandleClientDisconnection(ENetPeer* client) {
    // Remove the client from the members list.
    std::string nickname;
    {
        std::lock_guard lock{member_mutex};
        auto member{std::find_if(members.begin(), members.end(),
                                 [client](const Member& member) { return member.peer == client; })};
        if (member != members.end()) {
            nickname = member->nickname;
            members.erase(member);
        }
    }
    // Announce the change to all clients.
    enet_peer_disconnect(client, 0);
    if (!nickname.empty())
        SendStatusMessage(IDMemberLeft, nickname);
    BroadcastRoomInformation();
}

Common::WebResult Room::RoomImpl::MakeRequest(const std::string& method, const std::string& body) {
    auto response{method == "GET" ? client->Get("/lobby")
                                  : client->Post("/lobby", body, "application/json")};
    if (!response) {
        LOG_ERROR(Network, "Request returned null ({})", response->status);
        return Common::WebResult{Common::WebResult::Code::LibError, "Null response"};
    }
    if (response->status >= 400) {
        LOG_ERROR(Network, "Request returned error status code: {}", response->status);
        return Common::WebResult{Common::WebResult::Code::HttpError,
                                 std::to_string(response->status)};
    }
    auto content_type{response->headers.find("Content-Type")};
    if (content_type == response->headers.end()) {
        LOG_ERROR(Network, "Request returned no content");
        return Common::WebResult{Common::WebResult::Code::WrongContent, "No content"};
    }
    if (content_type->second.find("application/json") == std::string::npos &&
        content_type->second.find("text/html") == std::string::npos &&
        content_type->second.find("text/plain") == std::string::npos) {
        LOG_ERROR(Network, "Request returned wrong content: {}", content_type->second);
        return Common::WebResult{Common::WebResult::Code::WrongContent, "Wrong content"};
    }
    if (response->body == "You need to open both TCP & UDP ports to create a public room.")
        return Common::WebResult{Common::WebResult::Code::HttpError, response->body};
    return Common::WebResult{Common::WebResult::Code::Success, "", response->body};
}

std::vector<JSONRoom> Room::RoomImpl::GetRoomList() {
    auto reply{MakeRequest("GET").returned_data};
    if (reply.empty())
        return {};
    return nlohmann::json::parse(reply).get<std::vector<JSONRoom>>();
}

void Room::RoomImpl::UpdateAPIInformation() {
    std::lock_guard lock{member_mutex};
    JSONRoom room;
    room.name = room_information.name;
    room.creator = room_information.creator;
    room.description = room_information.description;
    room.port = room_information.port;
    room.max_members = room_information.max_members;
    room.net_version = Network::NetworkVersion;
    room.has_password = !password.empty();
    for (const auto& member : members)
        room.members.emplace_back(
            JSONRoom::Member{member.nickname, member.program, member.mac_address});
    nlohmann::json json = room;
    auto result{MakeRequest("POST", json.dump())};
    if (result.result_code != Common::WebResult::Code::Success &&
        is_public.load(std::memory_order_relaxed) && error_callback)
        return error_callback(result);
}

// Room
Room::Room() : room_impl{std::make_unique<RoomImpl>()} {}
Room::~Room() = default;

bool Room::Create(bool is_public, const std::string& name, const std::string& description,
                  const std::string& creator, u16 port, const std::string& password,
                  const u32 max_connections, const Room::BanList& ban_list) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;
    // In order to send the room is full message to the connecting client, we need to leave one slot
    // open so enet won't reject the incoming connection without telling us
    room_impl->server = enet_host_create(&address, max_connections + 1, NumChannels, 0, 0);
    if (!room_impl->server)
        return false;
    room_impl->is_open.store(true, std::memory_order_relaxed);
    room_impl->room_information.name = name;
    room_impl->room_information.creator = creator;
    room_impl->room_information.description = description;
    room_impl->room_information.max_members = max_connections;
    room_impl->room_information.port = port;
    room_impl->password = password;
    room_impl->ban_list = ban_list;
    room_impl->is_public.store(is_public, std::memory_order_relaxed);
    room_impl->StartLoop();
    if (room_impl->is_public.load(std::memory_order_relaxed)) {
        std::thread(
            [this] { room_impl->http_server.listen("0.0.0.0", room_impl->room_information.port); })
            .detach();
        room_impl->UpdateAPIInformation();
    }
    return true;
}

bool Room::IsOpen() const {
    return room_impl->is_open.load(std::memory_order_relaxed);
}

const RoomInformation& Room::GetRoomInformation() const {
    return room_impl->room_information;
}

Room::BanList Room::GetBanList() const {
    std::lock_guard lock{room_impl->ban_list_mutex};
    return room_impl->ban_list;
}

std::vector<Room::Member> Room::GetRoomMemberList() const {
    std::vector<Room::Member> member_list;
    std::lock_guard lock{room_impl->member_mutex};
    for (const auto& member_impl : room_impl->members) {
        Member member;
        member.nickname = member_impl.nickname;
        member.mac_address = member_impl.mac_address;
        member.program = member_impl.program;
        member_list.push_back(member);
    }
    return member_list;
}

bool Room::HasPassword() const {
    return !room_impl->password.empty();
}

void Room::Destroy() {
    room_impl->is_open.store(false, std::memory_order_relaxed);
    room_impl->room_thread->join();
    room_impl->room_thread.reset();
    if (room_impl->server)
        enet_host_destroy(room_impl->server);
    room_impl->room_information = {};
    room_impl->server = nullptr;
    {
        std::lock_guard lock{room_impl->member_mutex};
        room_impl->members.clear();
    }
    room_impl->room_information.max_members = 0;
    room_impl->room_information.name.clear();
    if (room_impl->is_public.load(std::memory_order_relaxed)) {
        nlohmann::json json;
        json["delete"] = room_impl->room_information.port;
        room_impl->MakeRequest("POST", json.dump());
    }
    room_impl->http_server.stop();
}

std::vector<JSONRoom> Room::GetRoomList() {
    return room_impl->GetRoomList();
}

void Room::SetErrorCallback(ErrorCallback cb) {
    return room_impl->SetErrorCallback(cb);
}

void Room::StopAnnouncing() {
    room_impl->is_public.store(false, std::memory_order_relaxed);
    room_impl->http_server.stop();
}

bool Room::IsPublic() const {
    return room_impl->is_public.load(std::memory_order_relaxed);
}

} // namespace Network
