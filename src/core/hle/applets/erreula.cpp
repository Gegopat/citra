// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <condition_variable>
#include <mutex>
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend.h"
#include "core/hle/applets/erreula.h"
#include "core/hle/service/cfg/cfg.h"

namespace HLE::Applets {

ResultCode ErrEula::ReceiveParameter(const Service::APT::MessageParameter& parameter) {
    if (parameter.signal != Service::APT::SignalType::Request) {
        LOG_ERROR(Applet_ErrEula, "unsupported signal {}", static_cast<u32>(parameter.signal));
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
    // TODO: allocated memory never released
    using Kernel::MemoryPermission;
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = manager.System().Kernel().CreateSharedMemoryForApplet(
        0, capture_info.size, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
        "ErrEula Shared Memory");
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

ResultCode ErrEula::StartImpl(const Service::APT::AppletStartupParameter& parameter) {
    is_running = true;
    std::memcpy(&config, parameter.buffer.data(), parameter.buffer.size());
    return RESULT_SUCCESS;
}

void ErrEula::Update() {
    manager.System().GetFrontend().LaunchErrEula(config, is_running);
    std::mutex m;
    std::unique_lock lock{m};
    std::condition_variable cv;
    cv.wait(lock, [this]() -> bool { return !is_running; });
    Finalize();
}

void ErrEula::Finalize() {
    // Let the program know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(config));
    std::memcpy(message.buffer.data(), &config, message.buffer.size());
    message.signal = Service::APT::SignalType::WakeupByExit;
    message.destination_id = AppletID::Program;
    message.sender_id = id;
    SendParameter(message);
    is_running = false;
}

} // namespace HLE::Applets
