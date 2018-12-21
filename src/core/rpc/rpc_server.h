// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
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

    void Notify();

private:
    void Start();
    void Stop();
    void HandleReadMemory(Packet& packet, u32 address, u32 data_size);
    void HandleWriteMemory(u32 address, const u8* data, u32 data_size);
    void HandlePadState(u32 raw);
    void HandleTouchState(s16 x, s16 y, bool valid);
    void HandleMotionState(s16 x, s16 y, s16 z, s16 roll, s16 pitch, s16 yaw);
    void HandleCircleState(s16 x, s16 y);
    void HandleSetResolution(u16 resolution);
    void HandleSetProgram(const std::string& path);
    void HandleSetOverrideControls(bool pad, bool touch, bool motion, bool circle);
    void HandlePause();
    void HandleResume();
    void HandleRestart();
    void HandleSetSpeedLimit(u16 speed_limit);
    void HandleSetBackgroundColor(float r, float g, float b);
    void HandleSetScreenRefreshRate(float rate);
    void HandleAreButtonsPressed(Packet& packet, u32 buttons);
    void HandleSetFrameAdvancing(bool enabled);
    void HandleAdvanceFrame();
    void HandleGetCurrentFrame(Packet& packet);
    bool ValidatePacket(const PacketHeader& packet_header);
    void HandleSingleRequest(std::unique_ptr<Packet> request);
    void HandleRequestsLoop();

    Server server;
    Common::SPSCQueue<std::unique_ptr<Packet>> request_queue;
    std::thread request_handler_thread;

    Core::System& system;

    std::mutex mutex;
    std::condition_variable cv;
};

} // namespace RPC
