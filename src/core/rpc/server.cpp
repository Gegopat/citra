// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <functional>
#include <asl/SocketServer.h>
#include "core/core.h"
#include "core/rpc/packet.h"
#include "core/rpc/rpc_server.h"
#include "core/rpc/server.h"

namespace RPC {

class TcpServer : public asl::SocketServer {
public:
    explicit TcpServer(std::function<void(std::unique_ptr<Packet>)> new_request_callback)
        : new_request_callback{new_request_callback} {}

    void serve(asl::Socket client) {
        const auto send_all{[&client](void* data, std::size_t length) {
            u8* ptr{static_cast<u8*>(data)};
            while (length > 0) {
                int i{client.write(data, length)};
                if (i < 1)
                    return false;
                ptr += i;
                length -= i;
            }
            return true;
        }};
        while (!client.disconnected()) {
            auto header_buffer{client.read(MIN_PACKET_SIZE)};
            PacketHeader header;
            std::memcpy(&header, header_buffer.ptr(), sizeof(header));
            std::vector<u8> data(header.packet_size);
            if (header.packet_size != 0)
                client.read(&data[0], header.packet_size);
            std::unique_ptr<Packet> new_packet{
                std::make_unique<Packet>(header, data.data(), [&](Packet& reply_packet) {
                    auto reply_header{reply_packet.GetHeader()};
                    send_all(&reply_header, sizeof(reply_header));
                    send_all(reply_packet.GetPacketData().data(), reply_packet.GetPacketDataSize());
                    LOG_INFO(RPC, "Sent reply (version={}, type={}, size={})",
                             reply_packet.GetVersion(),
                             static_cast<u32>(reply_packet.GetPacketType()),
                             reply_packet.GetPacketDataSize());
                })};
            // Send the request to the upper layer for handling
            new_request_callback(std::move(new_packet));
        }
    }

private:
    std::function<void(std::unique_ptr<Packet>)> new_request_callback;
};

struct Server::Impl {
    explicit Impl(std::function<void(std::unique_ptr<Packet>)> callback);
    ~Impl();

    void SendReply(Packet& request);

    std::atomic_bool running{true};

    TcpServer server;

    std::function<void(std::unique_ptr<Packet>)> new_request_callback;
};

Server::Impl::Impl(std::function<void(std::unique_ptr<Packet>)> callback) : server{callback} {
    new_request_callback = std::move(callback);
    // Use a random high port
    // TODO: Make configurable or increment port number on failure
    server.bind(45987);
    server.start(true);
    LOG_INFO(RPC, "Server listening on port 45987");
}

Server::Impl::~Impl() {
    running = false;
    server.stop();
    server.waitForStop();
    new_request_callback({});
}

Server::Server(RPCServer& rpc_server) : rpc_server{rpc_server} {}

void Server::Start() {
    const auto callback{[this](std::unique_ptr<RPC::Packet> new_request) {
        NewRequestCallback(std::move(new_request));
    }};
    try {
        impl = std::make_unique<Impl>(callback);
    } catch (...) {
        LOG_ERROR(RPC, "Error starting server");
    }
}

void Server::Stop() {
    impl.reset();
    LOG_INFO(RPC, "Server stopped");
}

void Server::NewRequestCallback(std::unique_ptr<RPC::Packet> new_request) {
    if (new_request)
        LOG_TRACE(RPC, "Received request (version={}, type={}, size={})", new_request->GetVersion(),
                  static_cast<u32>(new_request->GetPacketType()), new_request->GetPacketDataSize());
    rpc_server.QueueRequest(std::move(new_request));
}

} // namespace RPC
