// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
} // namespace Core

namespace Kernel {
class Mutex;
class SharedMemory;
} // namespace Kernel

namespace Service::APT {

class AppletManager;

/// Each APT service can only have up to 2 sessions connected at the same time.
static const u32 MaxAPTSessions{2};

/// Used by the program to pass information about the current framebuffer to applets.
struct CaptureBufferInfo {
    u32_le size;
    u8 is_3d;
    INSERT_PADDING_BYTES(0x3); // Padding for alignment
    u32_le top_screen_left_offset;
    u32_le top_screen_right_offset;
    u32_le top_screen_format;
    u32_le bottom_screen_left_offset;
    u32_le bottom_screen_right_offset;
    u32_le bottom_screen_format;
};
static_assert(sizeof(CaptureBufferInfo) == 0x20, "CaptureBufferInfo struct has incorrect size");

enum class QueryReply : u32 {
    ReplyReject = 0,
    ReplyAccept = 1,
    ReplyLater = 2,
};

enum class StartupArgumentType : u32 {
    OtherApp = 0,
    Restart = 1,
    OtherMedia = 2,
};

enum class ScreencapPostPermission : u32 {
    CleanThePermission = 0, // TODO: verify what "zero" means
    NoExplicitSetting = 1,
    EnableScreenshotPostingToMiiverse = 2,
    DisableScreenshotPostingToMiiverse = 3
};

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> apt, const char* name, u32 max_session);
        ~Interface();

    protected:
        void Initialize(Kernel::HLERequestContext& ctx);
        void GetSharedFont(Kernel::HLERequestContext& ctx);
        void Wrap(Kernel::HLERequestContext& ctx);
        void Unwrap(Kernel::HLERequestContext& ctx);
        void NotifyToWait(Kernel::HLERequestContext& ctx);
        void GetLockHandle(Kernel::HLERequestContext& ctx);
        void Enable(Kernel::HLERequestContext& ctx);
        void GetAppletManInfo(Kernel::HLERequestContext& ctx);
        void GetAppletInfo(Kernel::HLERequestContext& ctx);
        void IsRegistered(Kernel::HLERequestContext& ctx);
        void InquireNotification(Kernel::HLERequestContext& ctx);
        void SendParameter(Kernel::HLERequestContext& ctx);
        void ReceiveParameter(Kernel::HLERequestContext& ctx);
        void GlanceParameter(Kernel::HLERequestContext& ctx);
        void CancelParameter(Kernel::HLERequestContext& ctx);
        void PrepareToStartApplication(Kernel::HLERequestContext& ctx);
        void StartApplication(Kernel::HLERequestContext& ctx);
        void AppletUtility(Kernel::HLERequestContext& ctx);
        void SetAppCpuTimeLimit(Kernel::HLERequestContext& ctx);
        void GetAppCpuTimeLimit(Kernel::HLERequestContext& ctx);
        void PrepareToStartLibraryApplet(Kernel::HLERequestContext& ctx);
        void PrepareToStartNewestHomeMenu(Kernel::HLERequestContext& ctx);
        void PreloadLibraryApplet(Kernel::HLERequestContext& ctx);
        void FinishPreloadingLibraryApplet(Kernel::HLERequestContext& ctx);
        void StartLibraryApplet(Kernel::HLERequestContext& ctx);
        void CloseApplication(Kernel::HLERequestContext& ctx);
        void PrepareToDoApplicationJump(Kernel::HLERequestContext& ctx);
        void DoApplicationJump(Kernel::HLERequestContext& ctx);
        void CancelLibraryApplet(Kernel::HLERequestContext& ctx);
        void PrepareToCloseLibraryApplet(Kernel::HLERequestContext& ctx);
        void CloseLibraryApplet(Kernel::HLERequestContext& ctx);
        void SendDspSleep(Kernel::HLERequestContext& ctx);
        void SendDspWakeUp(Kernel::HLERequestContext& ctx);
        void SendCaptureBufferInfo(Kernel::HLERequestContext& ctx);
        void ReceiveCaptureBufferInfo(Kernel::HLERequestContext& ctx);
        void SleepSystem(Kernel::HLERequestContext& ctx);
        void GetStartupArgument(Kernel::HLERequestContext& ctx);
        void SetScreenCapPostPermission(Kernel::HLERequestContext& ctx);
        void GetScreenCapPostPermission(Kernel::HLERequestContext& ctx);
        void CheckNew3DSApp(Kernel::HLERequestContext& ctx);
        void CheckNew3DS(Kernel::HLERequestContext& ctx);
        void IsStandardMemoryLayout(Kernel::HLERequestContext& ctx);
        void ReplySleepQuery(Kernel::HLERequestContext& ctx);
        void SendDeliverArg(Kernel::HLERequestContext& ctx);
        void ReceiveDeliverArg(Kernel::HLERequestContext& ctx);
        void GetProgramID(Kernel::HLERequestContext& ctx);
        void IsTitleAllowed(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> apt;
    };

private:
    bool LoadSharedFont();

    /// Handle to shared memory region for shared system font
    Kernel::SharedPtr<Kernel::SharedMemory> shared_font_mem;
    bool shared_font_loaded{};
    bool shared_font_relocated{};

    Kernel::SharedPtr<Kernel::Mutex> lock;

    u32 cpu_percent{}; ///< CPU time available to the running program

    // APT::CheckNew3DSApp will check this unknown_ns_state_field to determine processing mode
    u8 unknown_ns_state_field{};

    std::vector<u8> screen_capture_buffer;

    ScreencapPostPermission screen_capture_post_permission{
        ScreencapPostPermission::CleanThePermission}; // TODO: verify the initial value

    std::shared_ptr<AppletManager> applet_manager;

    bool program_restart{};
    u64 jump_program_id;
    FS::MediaType jump_media;

    Core::System& system;
};

} // namespace Service::APT

namespace Service::NS {

/// Loads and launches the program identified by program_id in the specified media type.
Kernel::SharedPtr<Kernel::Process> Launch(Core::System& system, FS::MediaType media_type,
                                          u64 program_id);

void InstallInterfaces(Core::System& system);

} // namespace Service::NS
