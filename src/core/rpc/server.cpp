// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <functional>
#include <thread>
#define ZMQ_STATIC
#include <zmq.hpp>
#include "core/core.h"
#include "core/rpc/packet.h"
#include "core/rpc/rpc_server.h"
#include "core/rpc/server.h"

namespace RPC {

struct Server::Impl {
    Impl(std::function<void(std::unique_ptr<Packet>)> callback);
    ~Impl();

    void WorkerLoop();
    void SendReply(Packet& request);

    std::thread worker_thread;
    std::atomic_bool running{true};

    std::unique_ptr<zmq::context_t> zmq_context;
    std::unique_ptr<zmq::socket_t> zmq_socket;

    std::function<void(std::unique_ptr<Packet>)> new_request_callback;
};

Server::Impl::Impl(std::function<void(std::unique_ptr<Packet>)> callback) {
    zmq_context = std::make_unique<zmq::context_t>(1);
    zmq_socket = std::make_unique<zmq::socket_t>(*zmq_context, ZMQ_REP),
    new_request_callback = std::move(callback);
    // Use a random high port
    // TODO: Make configurable or increment port number on failure
    zmq_socket->bind("tcp://127.0.0.1:45987");
    LOG_INFO(RPC, "ZeroMQ listening on port 45987");
    worker_thread = std::thread(&Server::Impl::WorkerLoop, this);
}

Server::Impl::~Impl() {
    // Triggering the zmq_context destructor will cancel
    // any blocking calls to zmq_socket->recv()
    running = false;
    zmq_context.reset();
    worker_thread.join();
}

void Server::Impl::WorkerLoop() {
    zmq::message_t request;
    while (running) {
        try {
            if (zmq_socket->recv(&request, 0)) {
                if (request.size() >= MIN_PACKET_SIZE) {
                    u8* request_buffer{static_cast<u8*>(request.data())};
                    PacketHeader header;
                    std::memcpy(&header, request_buffer, sizeof(header));
                    if ((request.size() - MIN_PACKET_SIZE) == header.packet_size) {
                        u8* data{request_buffer + MIN_PACKET_SIZE};
                        std::function<void(Packet&)> send_reply_callback{
                            std::bind(&Server::Impl::SendReply, this, std::placeholders::_1)};
                        std::unique_ptr<Packet> new_packet{
                            std::make_unique<Packet>(header, data, send_reply_callback)};
                        // Send the request to the upper layer for handling
                        new_request_callback(std::move(new_packet));
                    }
                }
            }
        } catch (...) {
            LOG_WARNING(RPC, "Failed to receive data on ZeroMQ socket");
        }
    }
    new_request_callback({});
    // Destroying the socket must be done by this thread.
    zmq_socket.reset();
}

void Server::Impl::SendReply(Packet& reply_packet) {
    if (running) {
        auto reply_buffer{
            std::make_unique<u8[]>(MIN_PACKET_SIZE + reply_packet.GetPacketDataSize())};
        auto reply_header{reply_packet.GetHeader()};
        std::memcpy(reply_buffer.get(), &reply_header, sizeof(reply_header));
        std::memcpy(reply_buffer.get() + (4 * sizeof(u32)), reply_packet.GetPacketData().data(),
                    reply_packet.GetPacketDataSize());
        zmq_socket->send(reply_buffer.get(), MIN_PACKET_SIZE + reply_packet.GetPacketDataSize());
        LOG_INFO(RPC, "Sent reply version({}) id=({}) type=({}) size=({})",
                 reply_packet.GetVersion(), reply_packet.GetID(),
                 static_cast<u32>(reply_packet.GetPacketType()), reply_packet.GetPacketDataSize());
    }
}

Server::Server(RPCServer& rpc_server) : rpc_server{rpc_server} {}

void Server::Start() {
    const auto callback{[this](std::unique_ptr<RPC::Packet> new_request) {
        NewRequestCallback(std::move(new_request));
    }};
    try {
        impl = std::make_unique<Impl>(callback);
    } catch (...) {
        LOG_ERROR(RPC, "Error starting ZeroMQ server");
    }
}

void Server::Stop() {
    impl.reset();
    LOG_INFO(RPC, "ZeroMQ stopped");
}

void Server::NewRequestCallback(std::unique_ptr<RPC::Packet> new_request) {
    if (new_request)
        LOG_INFO(RPC, "Received request (version={}, id={}, type={}, size={})",
                 new_request->GetVersion(), new_request->GetID(),
                 static_cast<u32>(new_request->GetPacketType()), new_request->GetPacketDataSize());
    rpc_server.QueueRequest(std::move(new_request));
}

} // namespace RPC
