// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/applets/applet.h"
#include "core/hle/kernel/shared_memory.h"

namespace HLE::Applets {

class Mint final : public Applet {
public:
    explicit Mint(AppletID id, Service::APT::AppletManager& manager) : Applet{id, manager} {}

    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) override;
    ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) override;
    void Update() override;

private:
    /// This SharedMemory will be created when we receive the Request message.
    /// It holds the framebuffer info retrieved by the program with
    /// gsp::Gpu:ImportDisplayCaptureInfo
    Kernel::SharedPtr<Kernel::SharedMemory> framebuffer_memory;
};

} // namespace HLE::Applets
