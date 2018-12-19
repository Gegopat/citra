// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <thread>
#include "common/threadsafe_queue.h"
#include "core/rpc/server.h"

namespace Core {
class System;
} // namespace Core

namespace RPC {

class PacketHeader;

class RPCServer {
public:
    explicit RPCServer(Core::System& system);
    ~RPCServer();

    void QueueRequest(std::unique_ptr<RPC::Packet> request);

private:
    void Start();
    void Stop();
    void HandleReadMemory(Packet& packet, u32 address, u32 data_size);
    void HandleWriteMemory(Packet& packet, u32 address, const u8* data, u32 data_size);
    void HandlePadState(Packet& packet, u32 raw);
    void HandleTouchState(Packet& packet, s16 x, s16 y, bool valid);
    void HandleMotionState(Packet& packet, s16 x, s16 y, s16 z, s16 roll, s16 pitch, s16 yaw);
    void HandleCircleState(Packet& packet, s16 x, s16 y);
    void HandleSetResolution(Packet& packet, u16 resolution);
    void HandleSetProgram(Packet& packet, const std::string& path);
    void HandleSetOverrideControls(Packet& packet, bool pad, bool touch, bool motion, bool circle);
    void HandlePause(Packet& packet);
    void HandleResume(Packet& packet);
    void HandleRestart(Packet& packet);
    void HandleSetSpeedLimit(Packet& packet, u16 speed_limit);
    void HandleSetBackgroundColor(Packet& packet, float r, float g, float b);
    void HandleSetScreenRefreshRate(Packet& packet, float rate);
    void HandleAreButtonsPressed(Packet& packet, u32 buttons);
    void HandleSetFrameAdvancing(Packet& packet, bool enable);
    void HandleAdvanceFrame(Packet& packet);
    void HandleGetCurrentFrame(Packet& packet);
    bool ValidatePacket(const PacketHeader& packet_header);
    void HandleSingleRequest(std::unique_ptr<Packet> request);
    void HandleRequestsLoop();

    Server server;
    Common::SPSCQueue<std::unique_ptr<Packet>> request_queue;
    std::thread request_handler_thread;

    Core::System& system;
};

} // namespace RPC
