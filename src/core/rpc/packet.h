// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <vector>
#include "common/common_types.h"

namespace RPC {

enum class PacketType {
    Undefined = 0,
    ReadMemory,
    WriteMemory,
    PadState,
    TouchState,
    MotionState,
    CircleState,
    SetResolution,
    SetProgram,
    SetOverrideControls,
    Pause,
    Resume,
    Restart,
    SetSpeedLimit,
    SetBackgroundColor,
    SetScreenRefreshRate,
    IsButtonPressed,
    SetFrameAdvancing,
    AdvanceFrame,
    GetCurrentFrame,
};

struct PacketHeader {
    u32 version;
    u32 id;
    PacketType packet_type;
    u32 packet_size;
};

constexpr u32 CURRENT_VERSION{1};
constexpr u32 MIN_PACKET_SIZE{sizeof(PacketHeader)};
constexpr u32 MAX_READ_WRITE_SIZE{32};

class Packet {
public:
    Packet(const PacketHeader& header, u8* data, std::function<void(Packet&)> send_reply_callback);

    u32 GetVersion() const {
        return header.version;
    }

    u32 GetID() const {
        return header.id;
    }

    PacketType GetPacketType() const {
        return header.packet_type;
    }

    u32 GetPacketDataSize() const {
        return header.packet_size;
    }

    const PacketHeader& GetHeader() const {
        return header;
    }

    std::vector<u8>& GetPacketData() {
        return packet_data;
    }

    void SetPacketDataSize(u32 size) {
        header.packet_size = size;
    }

    void SendReply() {
        send_reply_callback(*this);
    }

private:
    struct PacketHeader header;
    std::vector<u8> packet_data;

    std::function<void(Packet&)> send_reply_callback;
};

} // namespace RPC
