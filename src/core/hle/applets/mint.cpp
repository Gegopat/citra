// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/applets/mint.h"
#include "core/hle/service/ns/ns.h"

namespace HLE::Applets {

ResultCode Mint::ReceiveParameter(const Service::APT::MessageParameter& parameter) {
    if (parameter.signal != Service::APT::SignalType::Request) {
        LOG_ERROR(Applet_Mint, "unsupported signal {}", static_cast<u32>(parameter.signal));
        UNIMPLEMENTED();
        // TODO: Find the right error code
        return ResultCode(-1);
    }
    // The Request message contains a buffer with the size of the framebuffer shared
    // memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer.size());
    std::memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));
    // TODO: allocated memory never released
    using Kernel::MemoryPermission;
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = manager.System().Kernel().CreateSharedMemoryForApplet(
        0, capture_info.size, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
        "Mint Shared Memory");
    // Send the response message with the newly created SharedMemory
    Service::APT::MessageParameter result;
    result.signal = Service::APT::SignalType::Response;
    result.buffer.clear();
    result.destination_id = AppletId::Program;
    result.sender_id = id;
    result.object = framebuffer_memory;
    SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode Mint::StartImpl(const Service::APT::AppletStartupParameter& parameter) {
    is_running = true;
    // TODO: Set the expected fields in the response buffer before resending it to the
    // program.
    // TODO: Reverse the parameter format for the Mint applet
    // Let the program know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(parameter.buffer.size());
    std::fill(message.buffer.begin(), message.buffer.end(), 0);
    message.signal = Service::APT::SignalType::WakeupByExit;
    message.destination_id = AppletId::Program;
    message.sender_id = id;
    SendParameter(message);
    is_running = false;
    return RESULT_SUCCESS;
}

void Mint::Update() {}

} // namespace HLE::Applets
