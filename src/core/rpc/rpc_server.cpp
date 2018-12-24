// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/core.h"
#include "core/cpu/cpu.h"
#include "core/frontend.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/hid/hid.h"
#include "core/memory.h"
#include "core/rpc/packet.h"
#include "core/rpc/rpc_server.h"
#include "video_core/video_core.h"

namespace RPC {

RPCServer::RPCServer(Core::System& system) : server{*this}, system{system} {
    Start();
    LOG_INFO(RPC, "Started");
}

RPCServer::~RPCServer() {
    Stop();
    LOG_INFO(RPC, "Stopped");
}

void RPCServer::HandleReadMemory(Packet& packet, u32 address, u32 data_size) {
    // Note: Memory read occurs asynchronously from the state of the emulator
    system.Memory().ReadBlock(*system.Kernel().GetCurrentProcess(), address,
                              packet.GetPacketData().data(), data_size);
    packet.SetPacketDataSize(data_size);
    packet.SendReply();
}

void RPCServer::HandleWriteMemory(u32 address, const u8* data, u32 data_size) {
    // Note: Memory write occurs asynchronously from the state of the emulator
    system.Memory().WriteBlock(*system.Kernel().GetCurrentProcess(), address, data, data_size);
    // If the memory happens to be executable code, make sure the changes become visible
    system.CPU().InvalidateCacheRange(address, data_size);
}

void RPCServer::HandlePadState(u32 raw) {
    system.ServiceManager()
        .GetService<Service::HID::Module::Interface>("hid:USER")
        ->GetModule()
        ->SetPadState(raw);
}

void RPCServer::HandleTouchState(s16 x, s16 y, bool valid) {
    system.ServiceManager()
        .GetService<Service::HID::Module::Interface>("hid:USER")
        ->GetModule()
        ->SetTouchState(x, y, valid);
}

void RPCServer::HandleMotionState(s16 x, s16 y, s16 z, s16 roll, s16 pitch, s16 yaw) {
    system.ServiceManager()
        .GetService<Service::HID::Module::Interface>("hid:USER")
        ->GetModule()
        ->SetMotionState(x, y, z, roll, pitch, yaw);
}

void RPCServer::HandleCircleState(s16 x, s16 y) {
    system.ServiceManager()
        .GetService<Service::HID::Module::Interface>("hid:USER")
        ->GetModule()
        ->SetCircleState(x, y);
}

void RPCServer::HandleSetResolution(u16 resolution) {
    Settings::values.resolution_factor = resolution;
}

void RPCServer::HandleSetProgram(const std::string& path) {
    system.SetProgram(path);
    std::unique_lock lock{mutex};
    cv.wait(lock);
}

void RPCServer::HandleSetOverrideControls(bool pad, bool touch, bool motion, bool circle) {
    system.ServiceManager()
        .GetService<Service::HID::Module::Interface>("hid:USER")
        ->GetModule()
        ->SetOverrideControls(pad, touch, motion, circle);
}

void RPCServer::HandlePause() {
    system.SetRunning(false);
}

void RPCServer::HandleResume() {
    system.SetRunning(true);
}

void RPCServer::HandleRestart() {
    system.Restart();
    std::unique_lock lock{mutex};
    cv.wait(lock);
}

void RPCServer::HandleSetSpeedLimit(u16 speed_limit) {
    Settings::values.use_frame_limit = true;
    Settings::values.frame_limit = speed_limit;
}

void RPCServer::HandleSetBackgroundColor(float r, float g, float b) {
    Settings::values.bg_red = r;
    Settings::values.bg_green = g;
    Settings::values.bg_blue = b;
    Settings::Apply(system);
}

void RPCServer::HandleSetScreenRefreshRate(float rate) {
    Settings::values.screen_refresh_rate = rate;
}

void RPCServer::HandleAreButtonsPressed(Packet& packet, u32 buttons) {
    packet.SetPacketDataSize(sizeof(bool));
    packet.GetPacketData()[0] = (system.ServiceManager()
                                     .GetService<Service::HID::Module::Interface>("hid:USER")
                                     ->GetModule()
                                     ->pad_state &
                                 buttons) != 0;
    packet.SendReply();
}

void RPCServer::HandleSetFrameAdvancing(bool enabled) {
    system.frame_limiter.SetFrameAdvancing(enabled);
    system.GetFrontend().UpdateFrameAdvancing();
}

void RPCServer::HandleAdvanceFrame() {
    system.frame_limiter.AdvanceFrame();
    system.GetFrontend().UpdateFrameAdvancing();
}

void RPCServer::HandleGetCurrentFrame(Packet& packet) {
    const auto& layout{system.GetFrontend().GetFramebufferLayout()};
    const auto size{(layout.width * layout.height) * sizeof(u32)};
    std::vector<u8> data(size);
    std::condition_variable cv;
    std::mutex m;
    std::unique_lock lock{m};
    VideoCore::RequestScreenshot(data.data(), [&cv] { cv.notify_one(); }, layout);
    cv.wait(lock);
    packet.SetPacketDataSize(static_cast<u32>(size));
    packet.GetPacketData() = std::move(data);
    packet.SendReply();
}

bool RPCServer::ValidatePacket(const PacketHeader& packet_header) {
    if (packet_header.version == CURRENT_VERSION) {
        switch (packet_header.packet_type) {
        case PacketType::ReadMemory:
        case PacketType::WriteMemory:
        case PacketType::PadState:
        case PacketType::TouchState:
        case PacketType::MotionState:
        case PacketType::CircleState:
        case PacketType::SetResolution:
        case PacketType::SetProgram:
        case PacketType::SetOverrideControls:
        case PacketType::Pause:
        case PacketType::Resume:
        case PacketType::Restart:
        case PacketType::SetSpeedLimit:
        case PacketType::SetBackgroundColor:
        case PacketType::SetScreenRefreshRate:
        case PacketType::AreButtonsPressed:
        case PacketType::SetFrameAdvancing:
        case PacketType::AdvanceFrame:
        case PacketType::GetCurrentFrame:
            if (packet_header.packet_size >= (sizeof(u32) * 2))
                return true;
            break;
        default:
            break;
        }
    }
    return false;
}

void RPCServer::HandleSingleRequest(std::unique_ptr<Packet> request_packet) {
    if (!system.IsPoweredOn()) {
        std::unique_lock lock{mutex};
        cv.wait(lock);
    }
    if (ValidatePacket(request_packet->GetHeader())) {
        // Currently, all request types use the address/data_size wire format
        u32 address;
        u32 data_size;
        std::memcpy(&address, request_packet->GetPacketData().data(), sizeof(address));
        std::memcpy(&data_size, request_packet->GetPacketData().data() + sizeof(address),
                    sizeof(data_size));
        switch (request_packet->GetPacketType()) {
        case PacketType::ReadMemory:
            if (data_size > 0 && data_size <= MAX_MEMORY_REQUEST_DATA_SIZE)
                HandleReadMemory(*request_packet, address, data_size);
            break;
        case PacketType::WriteMemory: {
            if (data_size > 0 && data_size <= MAX_MEMORY_REQUEST_DATA_SIZE) {
                const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
                HandleWriteMemory(address, data, data_size);
            }
            break;
        }
        case PacketType::PadState: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            u32 raw;
            std::memcpy(&raw, data, sizeof(raw));
            HandlePadState(raw);
            break;
        }
        case PacketType::TouchState: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            struct {
                s16 x;
                s16 y;
                bool valid;
            } state;
            std::memcpy(&state, data, sizeof(state));
            HandleTouchState(state.x, state.y, state.valid);
            break;
        }
        case PacketType::MotionState: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            struct {
                s16 x;
                s16 y;
                s16 z;
                s16 roll;
                s16 pitch;
                s16 yaw;
            } state;
            std::memcpy(&state, data, sizeof(state));
            HandleMotionState(state.x, state.y, state.z, state.roll, state.pitch, state.yaw);
            break;
        }
        case PacketType::CircleState: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            struct {
                s16 x;
                s16 y;
            } state;
            std::memcpy(&state, data, sizeof(state));
            HandleCircleState(state.x, state.y);
            break;
        }
        case PacketType::SetResolution: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            u16 resolution;
            std::memcpy(&resolution, data, sizeof(u16));
            HandleSetResolution(resolution);
            break;
        }
        case PacketType::SetProgram: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            HandleSetProgram(std::string{reinterpret_cast<const char*>(data)});
            break;
        }
        case PacketType::SetOverrideControls: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            struct {
                bool pad;
                bool touch;
                bool motion;
                bool circle;
            } state;
            std::memcpy(&state, data, sizeof(state));
            HandleSetOverrideControls(state.pad, state.touch, state.motion, state.circle);
            break;
        }
        case PacketType::Pause:
            HandlePause();
            break;
        case PacketType::Resume:
            HandleResume();
            break;
        case PacketType::Restart:
            HandleRestart();
            break;
        case PacketType::SetSpeedLimit: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            u16 speed_limit;
            std::memcpy(&speed_limit, data, sizeof(speed_limit));
            HandleSetSpeedLimit(speed_limit);
            break;
        }
        case PacketType::SetBackgroundColor: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            struct {
                float r;
                float g;
                float b;
            } color;
            std::memcpy(&color, data, sizeof(color));
            HandleSetBackgroundColor(color.r, color.g, color.b);
            break;
        }
        case PacketType::SetScreenRefreshRate: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            float rate;
            std::memcpy(&rate, data, sizeof(rate));
            HandleSetScreenRefreshRate(rate);
            break;
        }
        case PacketType::AreButtonsPressed: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            u32 buttons;
            std::memcpy(&buttons, data, sizeof(buttons));
            HandleAreButtonsPressed(*request_packet, buttons);
            break;
        }
        case PacketType::SetFrameAdvancing: {
            const u8* data{request_packet->GetPacketData().data() + (sizeof(u32) * 2)};
            bool enabled;
            std::memcpy(&enabled, data, sizeof(enabled));
            HandleSetFrameAdvancing(enabled);
            break;
        }
        case PacketType::AdvanceFrame:
            HandleAdvanceFrame();
            break;
        case PacketType::GetCurrentFrame:
            HandleGetCurrentFrame(*request_packet);
            break;
        default:
            break;
        }
    }
}

void RPCServer::HandleRequestsLoop() {
    std::unique_ptr<RPC::Packet> request_packet;
    LOG_INFO(RPC, "Request handler started.");
    for (;;) {
        request_packet = request_queue.PopWait();
        if (!request_packet)
            break;
        HandleSingleRequest(std::move(request_packet));
    }
}

void RPCServer::QueueRequest(std::unique_ptr<RPC::Packet> request) {
    request_queue.Push(std::move(request));
}

void RPCServer::Start() {
    const auto f{[this]() { HandleRequestsLoop(); }};
    request_handler_thread = std::thread(f);
    server.Start();
}

void RPCServer::Stop() {
    server.Stop();
    request_handler_thread.join();
}

void RPCServer::Notify() {
    cv.notify_one();
}

} // namespace RPC
