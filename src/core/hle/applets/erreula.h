// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/applets/applet.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/ns/ns.h"

namespace HLE::Applets {

enum class ErrEulaErrorType : u32 {
    ErrorCode,
    ErrorText,
    Eula,
    EulaFirstBoot,
    EulaDrawOnly,
    Agree,
    LocalizedErrorText = ErrorText | 0x100,
};

enum class ErrEulaResult : s32 {
    Unknown = -1,
    None,
    Success,
    NotSupported,
    HomeButton = 10,
    SoftwareReset,
    PowerButton
};

struct ErrEulaConfig {
    ErrEulaErrorType error_type;
    u32 error_code;
    u16 upper_screen_flag;
    u16 use_language;
    std::array<u16, 1900> error_text;
    bool home_button;
    bool software_reset;
    bool program_jump;
    INSERT_PADDING_BYTES(137);
    ErrEulaResult return_code;
    u16 eula_version;
    INSERT_PADDING_BYTES(10);
};

static_assert(sizeof(ErrEulaConfig) == 0xF80, "ErrEulaConfig structure size is wrong");

class ErrEula final : public Applet {
public:
    explicit ErrEula(AppletId id, Service::APT::AppletManager& manager) : Applet{id, manager} {}

    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) override;
    ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) override;
    void Update() override;
    void Finalize();

private:
    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the program with
    /// gsp::Gpu:ImportDisplayCaptureInfo
    Kernel::SharedPtr<Kernel::SharedMemory> framebuffer_memory;

    ErrEulaConfig config;
};

} // namespace HLE::Applets
