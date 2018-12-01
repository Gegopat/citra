// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <future>
#include <httplib.h>
#include <json.hpp>
#include "common/logging/log.h"
#include "lobby/api.h"

namespace AnnounceMultiplayerRoom {

void to_json(nlohmann::json& json, const Room::Member& member) {
    json["name"] = member.name;
    json["program"] = member.program;
}

void from_json(const nlohmann::json& json, Room::Member& member) {
    member.name = json.at("name").get<std::string>();
    member.program = json.at("program").get<std::string>();
}

void to_json(nlohmann::json& json, const Room& room) {
    json["ip"] = room.ip;
    json["name"] = room.name;
    json["creator"] = room.creator;
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

void from_json(const nlohmann::json& json, Room& room) {
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
        room.members = json.at("members").get<std::vector<Room::Member>>();
    } catch (const nlohmann::detail::out_of_range& e) {
        LOG_DEBUG(Network, "Out of range {}", e.what());
    }
}

} // namespace AnnounceMultiplayerRoom

LobbyAPI::LobbyAPI()
    : client{std::make_shared<httplib::SSLClient>("citra-valentin-api.glitch.me", 443)} {}

void LobbyAPI::SetRoomInformation(const std::string& name, const u16 port,
                                  const std::string& creator, const std::string& description,
                                  const u32 max_members, const u32 net_version,
                                  const bool has_password) {
    room.name = name;
    room.description = description;
    room.port = port;
    room.creator = creator;
    room.max_members = max_members;
    room.net_version = net_version;
    room.has_password = has_password;
}

void LobbyAPI::AddMember(const std::string& nickname, const MACAddress& mac_address,
                         const std::string& program) {
    AnnounceMultiplayerRoom::Room::Member member;
    member.name = nickname;
    member.mac_address = mac_address;
    member.program = program;
    room.members.push_back(member);
}

Common::WebResult LobbyAPI::MakeRequest(const std::string& method, const std::string& body) {
    using namespace httplib;
    Headers headers;
    if (method != "GET")
        headers.emplace("Content-Type", "application/json");
    Request request;
    request.method = method;
    request.path = "/lobby";
    request.headers = headers;
    request.body = body;
    Response response;
    if (!client->send(request, response)) {
        LOG_ERROR(Network, "{} returned null ({})", method, response.status);
        return Common::WebResult{Common::WebResult::Code::LibError, "Null response"};
    }
    if (response.status >= 400) {
        LOG_ERROR(Network, "{} returned error status code: {}", method, response.status);
        return Common::WebResult{Common::WebResult::Code::HttpError,
                                 std::to_string(response.status)};
    }
    auto content_type{response.headers.find("Content-Type")};
    if (content_type == response.headers.end()) {
        LOG_ERROR(Network, "{} returned no content", method);
        return Common::WebResult{Common::WebResult::Code::WrongContent, ""};
    }
    if (content_type->second.find("application/json") == std::string::npos &&
        content_type->second.find("text/html; charset=utf-8") == std::string::npos) {
        LOG_ERROR(Network, "{} returned wrong content: {}", method, content_type->second);
        return Common::WebResult{Common::WebResult::Code::WrongContent, "Wrong content"};
    }
    return Common::WebResult{Common::WebResult::Code::Success, "", response.body};
}

Common::WebResult LobbyAPI::Announce() {
    nlohmann::json json = room;
    return MakeRequest("POST", json.dump());
}

void LobbyAPI::ClearMembers() {
    room.members.clear();
}

AnnounceMultiplayerRoom::RoomList LobbyAPI::GetRoomList() {
    auto reply{MakeRequest("GET").returned_data};
    if (reply.empty())
        return {};
    return nlohmann::json::parse(reply).get<AnnounceMultiplayerRoom::RoomList>();
}

void LobbyAPI::Delete() {
    nlohmann::json json;
    json["delete"] = room.port;
    MakeRequest("POST", json.dump());
}
