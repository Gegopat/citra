// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <condition_variable>
#include <cstring>
#include <mutex>
#include <boost/crc.hpp>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/frontend.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"

namespace HLE::Applets {

ResultCode MiiSelector::ReceiveParameter(const Service::APT::MessageParameter& parameter) {
    if (parameter.signal != Service::APT::SignalType::Request) {
        LOG_ERROR(Applet_MiiSelector, "unsupported signal {}", static_cast<u32>(parameter.signal));
        UNIMPLEMENTED();
        // TODO: Find the right error code
        return ResultCode(-1);
    }
    // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared
    // memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer.size());
    std::memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));
    using Kernel::MemoryPermission;
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = manager.System().Kernel().CreateSharedMemoryForApplet(
        0, capture_info.size, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
        "Mii Selector Shared Memory");
    // Send the response message with the newly created SharedMemory
    Service::APT::MessageParameter result;
    result.signal = Service::APT::SignalType::Response;
    result.buffer.clear();
    result.destination_id = AppletID::Program;
    result.sender_id = id;
    result.object = framebuffer_memory;
    SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode MiiSelector::StartImpl(const Service::APT::AppletStartupParameter& parameter) {
    is_running = true;
    std::memcpy(&config, parameter.buffer.data(), parameter.buffer.size());
    MiiResult result{};
    if (config.magic_value != MiiSelectorMagic)
        result.return_code = 1;
    else {
        manager.System().GetFrontend().LaunchMiiSelector(config, result, is_running);
        std::mutex m;
        std::unique_lock lock{m};
        std::condition_variable cv;
        cv.wait(lock, [this]() -> bool { return !is_running; });
        result.mii_data_checksum = boost::crc<16, 0x1021, 0, 0, false, false>(
            &result.selected_mii_data, sizeof(result.selected_mii_data) + sizeof(result.pad96));
    }
    // Let the program know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(MiiResult));
    std::memcpy(message.buffer.data(), &result, message.buffer.size());
    message.signal = Service::APT::SignalType::WakeupByExit;
    message.destination_id = AppletID::Program;
    message.sender_id = id;
    SendParameter(message);
    is_running = false;
    return RESULT_SUCCESS;
}

void MiiSelector::Update() {}

} // namespace HLE::Applets
