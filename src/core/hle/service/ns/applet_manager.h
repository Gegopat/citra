// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <optional>
#include <type_traits>
#include <vector>
#include "core/hle/applets/applet.h"
#include "core/hle/kernel/event.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"

namespace HLE::Applets {
class Applet;
} // namespace HLE::Applets

/// Applet id's used by APT functions
enum class AppletId : u32 {
    None = 0,
    AnySystemApplet = 0x100,
    HomeMenu = 0x101,
    AlternateMenu = 0x103,
    Camera = 0x110,
    FriendList = 0x112,
    GameNotes = 0x113,
    InternetBrowser = 0x114,
    InstructionManual = 0x115,
    Notifications = 0x116,
    Miiverse = 0x117,
    MiiversePost = 0x118,
    AmiiboSettings = 0x119,
    AnySysLibraryApplet = 0x200,
    SoftwareKeyboard1 = 0x201,
    Ed1 = 0x202,
    PnoteApp = 0x204,
    SnoteApp = 0x205,
    Error = 0x206,
    Mint = 0x207,
    Extrapad = 0x208,
    Memolib = 0x209,
    Program = 0x300,
    Tiger = 0x301,
    AnyLibraryApplet = 0x400,
    SoftwareKeyboard2 = 0x401,
    Ed2 = 0x402,
    PnoteApp2 = 0x404,
    SnoteApp2 = 0x405,
    Error2 = 0x406,
    Mint2 = 0x407,
    Extrapad2 = 0x408,
    Memolib2 = 0x409,
};

// Specializes std::hash for AppletId, so that we can use it in std::unordered_map.
// Workaround for libstdc++ bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60970
namespace std {

template <>
struct hash<AppletId> {
    using argument_type = AppletId;
    using result_type = size_t;

    result_type operator()(const argument_type& id_code) const {
        using Type = std::underlying_type<argument_type>::type;
        return std::hash<Type>()(static_cast<Type>(id_code));
    }
};

} // namespace std

namespace Service::APT {

/// Signals used by APT functions
enum class SignalType : u32 {
    None = 0x0,
    Wakeup = 0x1,
    Request = 0x2,
    Response = 0x3,
    Exit = 0x4,
    Message = 0x5,
    HomeButtonSingle = 0x6,
    HomeButtonDouble = 0x7,
    DspSleep = 0x8,
    DspWakeup = 0x9,
    WakeupByExit = 0xA,
    WakeupByPause = 0xB,
    WakeupByCancel = 0xC,
    WakeupByCancelAll = 0xD,
    WakeupByPowerButtonClick = 0xE,
    WakeupToJumpHome = 0xF,
    RequestForSysApplet = 0x10,
    WakeupToLaunchProgram = 0x11,
};

/// Holds information about the parameters used in Send/Glance/ReceiveParameter
struct MessageParameter {
    AppletId sender_id{AppletId::None};
    AppletId destination_id{AppletId::None};
    SignalType signal{SignalType::None};
    Kernel::SharedPtr<Kernel::Object> object{};
    std::vector<u8> buffer;
};

/// Holds information about the parameters used in StartLibraryApplet
struct AppletStartupParameter {
    Kernel::SharedPtr<Kernel::Object> object{};
    std::vector<u8> buffer;
};

union AppletAttributes {
    u32 raw;

    BitField<0, 3, u32> applet_pos;
    BitField<29, 1, u32> is_home_menu;

    AppletAttributes() : raw{0} {}
    AppletAttributes(u32 attributes) : raw{attributes} {}
};

class AppletManager : public std::enable_shared_from_this<AppletManager> {
public:
    explicit AppletManager(Core::System& system);
    ~AppletManager();

    /**
     * Clears any existing parameter and places a new one. This function is currently only used by
     * HLE Applets and should be likely removed in the future
     */
    void CancelAndSendParameter(const MessageParameter& parameter);

    ResultCode SendParameter(const MessageParameter& parameter);
    ResultVal<MessageParameter> GlanceParameter(AppletId program_id);
    ResultVal<MessageParameter> ReceiveParameter(AppletId program_id);
    bool CancelParameter(bool check_sender, AppletId sender_appid, bool check_receiver,
                         AppletId receiver_appid);

    struct InitializeResult {
        Kernel::SharedPtr<Kernel::Event> notification_event;
        Kernel::SharedPtr<Kernel::Event> parameter_event;
    };

    ResultVal<InitializeResult> Initialize(AppletId program_id, AppletAttributes attributes);
    ResultCode Enable(AppletAttributes attributes);
    bool IsRegistered(AppletId program_id);
    ResultCode PrepareToStartLibraryApplet(AppletId applet_id);
    ResultCode PreloadLibraryApplet(AppletId applet_id);
    ResultCode FinishPreloadingLibraryApplet(AppletId applet_id);
    ResultCode StartLibraryApplet(AppletId applet_id, Kernel::SharedPtr<Kernel::Object> object,
                                  const std::vector<u8>& buffer);
    ResultCode PrepareToCloseLibraryApplet(bool not_pause, bool exiting, bool jump_home);
    ResultCode CloseLibraryApplet(Kernel::SharedPtr<Kernel::Object> object, std::vector<u8> buffer);

    struct AppletInfo {
        u64 program_id;
        Service::FS::MediaType media_type;
        bool registered;
        bool loaded;
        u32 attributes;
    };

    ResultVal<AppletInfo> GetAppletInfo(AppletId program_id);

    void ScheduleEvent(AppletId id);

    Core::System& System() {
        return system;
    }

private:
    /// Parameter data to be returned in the next call to Glance/ReceiveParameter.
    std::optional<MessageParameter> next_parameter;

    static constexpr std::size_t NumAppletSlot{4};

    enum class AppletSlot : u8 {
        Program,
        SystemApplet,
        HomeMenu,
        LibraryApplet,

        // An invalid tag
        Error,
    };

    struct AppletSlotData {
        AppletId applet_id;
        AppletSlot slot;
        bool registered;
        bool loaded;
        AppletAttributes attributes;
        Kernel::SharedPtr<Kernel::Event> notification_event;
        Kernel::SharedPtr<Kernel::Event> parameter_event;

        void Reset() {
            applet_id = AppletId::None;
            registered = false;
            attributes.raw = 0;
        }
    };

    // Holds data about the concurrently running applets in the system.
    std::array<AppletSlotData, NumAppletSlot> applet_slots{};

    void AppletUpdateEvent(u64 applet_id, s64 cycles_late);
    u64 GetProgramIDForApplet(AppletId id);

    // This overload returns nullptr if no applet with the specified id has been started.
    AppletSlotData* GetAppletSlotData(AppletId id);
    AppletSlotData* GetAppletSlotData(AppletAttributes attributes);

    // Command that will be sent to the program when a library applet calls CloseLibraryApplet.
    SignalType library_applet_closing_command;

    std::unordered_map<AppletId, std::shared_ptr<HLE::Applets::Applet>> hle_applets;

    /// The CoreTiming event identifier for the Applet update callback.
    Core::TimingEventType* applet_update_event;

    Core::System& system;
};

} // namespace Service::APT
