// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/common_paths.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/applets/applet.h"
#include "core/hle/applets/erreula.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/applets/mint.h"
#include "core/hle/applets/swkbd.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/ns/applet_manager.h"
#include "core/hle/service/ns/errors.h"
#include "core/hle/service/ns/ns.h"
#include "core/settings.h"

namespace Service::APT {

/// The interval at which the Applet update callback will be called, 16.6ms
constexpr u64 applet_update_interval_us{16666};

enum class AppletPos { Program = 0, Library = 1, System = 2, SysLibrary = 3, Resident = 4 };

struct AppletTitleData {
    // There are two possible applet ids for each applet.
    std::array<AppletId, 2> applet_ids;

    // There's a specific ProgramID per region for each applet.
    static constexpr std::size_t NumRegions{7};
    std::array<u64, NumRegions> program_id_s;
};

constexpr std::size_t NumApplets{29};
constexpr std::array<AppletTitleData, NumApplets> applet_titleids{{
    {AppletId::HomeMenu, AppletId::None, 0x4003000008202, 0x4003000008F02, 0x4003000009802,
     0x4003000008202, 0x400300000A102, 0x400300000A902, 0x400300000B102},
    {AppletId::AlternateMenu, AppletId::None, 0x4003000008102, 0x4003000008102, 0x4003000008102,
     0x4003000008102, 0x4003000008102, 0x4003000008102, 0x4003000008102},
    {AppletId::Camera, AppletId::None, 0x4003000008402, 0x4003000009002, 0x4003000009902,
     0x4003000008402, 0x400300000A202, 0x400300000AA02, 0x400300000B202},
    {AppletId::FriendList, AppletId::None, 0x4003000008D02, 0x4003000009602, 0x4003000009F02,
     0x4003000008D02, 0x400300000A702, 0x400300000AF02, 0x400300000B702},
    {AppletId::GameNotes, AppletId::None, 0x4003000008702, 0x4003000009302, 0x4003000009C02,
     0x4003000008702, 0x400300000A502, 0x400300000AD02, 0x400300000B502},
    {AppletId::InternetBrowser, AppletId::None, 0x4003000008802, 0x4003000009402, 0x4003000009D02,
     0x4003000008802, 0x400300000A602, 0x400300000AE02, 0x400300000B602},
    {AppletId::InstructionManual, AppletId::None, 0x4003000008602, 0x4003000009202, 0x4003000009B02,
     0x4003000008602, 0x400300000A402, 0x400300000AC02, 0x400300000B402},
    {AppletId::Notifications, AppletId::None, 0x4003000008E02, 0x4003000009702, 0x400300000A002,
     0x4003000008E02, 0x400300000A802, 0x400300000B002, 0x400300000B802},
    {AppletId::Miiverse, AppletId::None, 0x400300000BC02, 0x400300000BD02, 0x400300000BE02,
     0x400300000BC02, 0x4003000009E02, 0x4003000009502, 0x400300000B902},
    // These values obtained from an older NS dump firmware 4.5
    {AppletId::MiiversePost, AppletId::None, 0x400300000BA02, 0x400300000BA02, 0x400300000BA02,
     0x400300000BA02, 0x400300000BA02, 0x400300000BA02, 0x400300000BA02},
    // {AppletId::MiiversePost, AppletId::None, 0x4003000008302, 0x4003000008B02, 0x400300000BA02,
    //  0x4003000008302, 0x0, 0x0, 0x0},
    {AppletId::AmiiboSettings, AppletId::None, 0x4003000009502, 0x4003000009E02, 0x400300000B902,
     0x4003000009502, 0x0, 0x4003000008C02, 0x400300000BF02},
    {AppletId::SoftwareKeyboard1, AppletId::SoftwareKeyboard2, 0x400300000C002, 0x400300000C802,
     0x400300000D002, 0x400300000C002, 0x400300000D802, 0x400300000DE02, 0x400300000E402},
    {AppletId::Ed1, AppletId::Ed2, 0x400300000C102, 0x400300000C902, 0x400300000D102,
     0x400300000C102, 0x400300000D902, 0x400300000DF02, 0x400300000E502},
    {AppletId::PnoteApp, AppletId::PnoteApp2, 0x400300000C302, 0x400300000CB02, 0x400300000D302,
     0x400300000C302, 0x400300000DB02, 0x400300000E102, 0x400300000E702},
    {AppletId::SnoteApp, AppletId::SnoteApp2, 0x400300000C402, 0x400300000CC02, 0x400300000D402,
     0x400300000C402, 0x400300000DC02, 0x400300000E202, 0x400300000E802},
    {AppletId::Error, AppletId::Error2, 0x400300000C502, 0x400300000C502, 0x400300000C502,
     0x400300000C502, 0x400300000CF02, 0x400300000CF02, 0x400300000CF02},
    {AppletId::Mint, AppletId::Mint2, 0x400300000C602, 0x400300000CE02, 0x400300000D602,
     0x400300000C602, 0x400300000DD02, 0x400300000E302, 0x400300000E902},
    {AppletId::Extrapad, AppletId::Extrapad2, 0x400300000CD02, 0x400300000CD02, 0x400300000CD02,
     0x400300000CD02, 0x400300000D502, 0x400300000D502, 0x400300000D502},
    {AppletId::Memolib, AppletId::Memolib2, 0x400300000F602, 0x400300000F602, 0x400300000F602,
     0x400300000F602, 0x400300000F602, 0x400300000F602, 0x400300000F602},
    // TODO: Fill in the rest of the titleids
}};

/// Handles updating the current Applet every time it's called.
void AppletManager::AppletUpdateEvent(u64 applet_id, s64 cycles_late) {
    auto id{static_cast<AppletId>(applet_id)};
    auto itr{hle_applets.find(id)};
    std::shared_ptr<HLE::Applets::Applet> applet{itr == hle_applets.end() ? nullptr : itr->second};
    ASSERT_MSG(applet, "Applet 0x{:03X} doesn't exist!", static_cast<u32>(id));
    applet->Update();
    // If the applet is still running after the last update, reschedule the event
    if (applet->IsRunning())
        system.CoreTiming().ScheduleEvent(usToCycles(applet_update_interval_us) - cycles_late,
                                          applet_update_event, applet_id);
}

u64 AppletManager::GetProgramIDForApplet(AppletId id) {
    ASSERT_MSG(id != AppletId::None, "Invalid applet id");
    auto itr{std::find_if(applet_titleids.begin(), applet_titleids.end(),
                          [id](const AppletTitleData& data) {
                              return data.applet_ids[0] == id || data.applet_ids[1] == id;
                          })};
    ASSERT_MSG(itr != applet_titleids.end(), "Unknown applet id {:#05X}", static_cast<u32>(id));
    return itr->program_id_s[system.ServiceManager()
                                 .GetService<CFG::Module::Interface>("cfg:u")
                                 ->GetModule()
                                 ->GetRegionValue()];
}

AppletManager::AppletSlotData* AppletManager::GetAppletSlotData(AppletId id) {
    auto GetSlot{[this](AppletSlot slot) -> AppletSlotData* {
        return &applet_slots[static_cast<std::size_t>(slot)];
    }};
    if (id == AppletId::Program) {
        auto slot{GetSlot(AppletSlot::Program)};
        if (slot->applet_id != AppletId::None)
            return slot;
        return nullptr;
    }
    if (id == AppletId::AnySystemApplet) {
        auto system_slot{GetSlot(AppletSlot::SystemApplet)};
        if (system_slot->applet_id != AppletId::None)
            return system_slot;
        // The Home Menu is also a system applet, but it lives in its own slot to be able to run
        // concurrently with other system applets.
        auto home_slot{GetSlot(AppletSlot::HomeMenu)};
        if (home_slot->applet_id != AppletId::None)
            return home_slot;
        return nullptr;
    }
    if (id == AppletId::AnyLibraryApplet || id == AppletId::AnySysLibraryApplet) {
        auto slot{GetSlot(AppletSlot::LibraryApplet)};
        if (slot->applet_id == AppletId::None)
            return nullptr;
        u32 applet_pos{slot->attributes.applet_pos};
        if (id == AppletId::AnyLibraryApplet && applet_pos == static_cast<u32>(AppletPos::Library))
            return slot;
        if (id == AppletId::AnySysLibraryApplet &&
            applet_pos == static_cast<u32>(AppletPos::SysLibrary))
            return slot;
        return nullptr;
    }
    if (id == AppletId::HomeMenu || id == AppletId::AlternateMenu) {
        auto slot{GetSlot(AppletSlot::HomeMenu)};
        if (slot->applet_id != AppletId::None)
            return slot;
        return nullptr;
    }
    for (auto& slot : applet_slots)
        if (slot.applet_id == id)
            return &slot;
    return nullptr;
}

AppletManager::AppletSlotData* AppletManager::GetAppletSlotData(AppletAttributes attributes) {
    // Mapping from AppletPos to AppletSlot
    constexpr std::array<AppletSlot, 6> applet_position_slots{
        {AppletSlot::Program, AppletSlot::LibraryApplet, AppletSlot::SystemApplet,
         AppletSlot::LibraryApplet, AppletSlot::Error, AppletSlot::LibraryApplet}};
    u32 applet_pos{attributes.applet_pos};
    if (applet_pos >= applet_position_slots.size())
        return nullptr;
    auto slot{applet_position_slots[applet_pos]};
    if (slot == AppletSlot::Error)
        return nullptr;
    // The Home Menu is a system applet, however, it has its own applet slot so that it can run
    // concurrently with other system applets.
    if (slot == AppletSlot::SystemApplet && attributes.is_home_menu)
        return &applet_slots[static_cast<std::size_t>(AppletSlot::HomeMenu)];
    return &applet_slots[static_cast<std::size_t>(slot)];
}

void AppletManager::CancelAndSendParameter(const MessageParameter& parameter) {
    next_parameter = parameter;
    // Signal the event to let the receiver know that a new parameter is ready to be read
    auto const slot_data{GetAppletSlotData(static_cast<AppletId>(parameter.destination_id))};
    if (!slot_data) {
        LOG_DEBUG(Service_APT, "No applet was registered with the id {:03X}",
                  static_cast<u32>(parameter.destination_id));
        return;
    }
    slot_data->parameter_event->Signal();
}

ResultCode AppletManager::SendParameter(const MessageParameter& parameter) {
    // A new parameter can not be sent if the previous one hasn't been consumed yet
    if (next_parameter)
        return ResultCode(ErrCodes::ParameterPresent, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    CancelAndSendParameter(parameter);
    if (Settings::values.use_lle_applets)
        return RESULT_SUCCESS;
    auto itr{hle_applets.find(parameter.destination_id)};
    if (itr != hle_applets.end())
        return itr->second->ReceiveParameter(parameter);
    return RESULT_SUCCESS;
}

ResultVal<MessageParameter> AppletManager::GlanceParameter(AppletId program_id) {
    if (!next_parameter)
        return ResultCode(ErrorDescription::NoData, ErrorModule::Applet, ErrorSummary::InvalidState,
                          ErrorLevel::Status);
    if (next_parameter->destination_id != program_id)
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                          ErrorLevel::Status);
    MessageParameter parameter{*next_parameter};
    // Note: The NS module always clears the DSPSleep and DSPWakeup signals even in GlanceParameter.
    if (next_parameter->signal == SignalType::DspSleep ||
        next_parameter->signal == SignalType::DspWakeup)
        next_parameter.reset();
    return MakeResult<MessageParameter>(parameter);
}

ResultVal<MessageParameter> AppletManager::ReceiveParameter(AppletId program_id) {
    auto result{GlanceParameter(program_id)};
    if (result.Succeeded())
        // Clear the parameter
        next_parameter.reset();
    return result;
}

bool AppletManager::CancelParameter(bool check_sender, AppletId sender_appid, bool check_receiver,
                                    AppletId receiver_appid) {
    bool cancellation_success{true};
    if (!next_parameter)
        cancellation_success = false;
    else {
        if (check_sender && next_parameter->sender_id != sender_appid)
            cancellation_success = false;
        if (check_receiver && next_parameter->destination_id != receiver_appid)
            cancellation_success = false;
    }
    if (cancellation_success)
        next_parameter.reset();
    return cancellation_success;
}

ResultVal<AppletManager::InitializeResult> AppletManager::Initialize(AppletId program_id,
                                                                     AppletAttributes attributes) {
    auto const slot_data{GetAppletSlotData(attributes)};
    // Note: The real NS service doesn't check if the attributes value is valid before accessing
    // the data in the array
    ASSERT_MSG(slot_data, "Invalid applet attributes");
    if (slot_data->registered)
        return ResultCode(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    slot_data->applet_id = static_cast<AppletId>(program_id);
    slot_data->attributes.raw = attributes.raw;
    if (slot_data->applet_id == AppletId::Program || slot_data->applet_id == AppletId::HomeMenu) {
        // Initialize the APT parameter to wake up the program.
        next_parameter.emplace();
        next_parameter->signal = SignalType::Wakeup;
        next_parameter->sender_id = AppletId::None;
        next_parameter->destination_id = program_id;
        // Not signaling the parameter event will cause the program (or Home Menu) to hang
        // during startup. In the real console, it is usually the Kernel and Home Menu who cause NS
        // to signal the HomeMenu and Program parameter events, respectively.
        slot_data->parameter_event->Signal();
    }
    return MakeResult<InitializeResult>(
        {slot_data->notification_event, slot_data->parameter_event});
}

ResultCode AppletManager::Enable(AppletAttributes attributes) {
    auto const slot_data{GetAppletSlotData(attributes)};
    if (!slot_data)
        return ResultCode(ErrCodes::InvalidAppletSlot, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    slot_data->registered = true;
    return RESULT_SUCCESS;
}

bool AppletManager::IsRegistered(AppletId program_id) {
    const auto slot_data{GetAppletSlotData(program_id)};
    // Check if an LLE applet was registered first, then fallback to HLE applets
    bool is_registered{slot_data && slot_data->registered};
    if (!is_registered && !Settings::values.use_lle_applets)
        if (program_id == AppletId::AnyLibraryApplet)
            is_registered =
                std::any_of(hle_applets.begin(), hle_applets.end(), [](const auto& applet) -> bool {
                    return applet.second->IsLibraryApplet();
                });
        else if (hle_applets.find(program_id) != hle_applets.end())
            // The applet exists, set it as registered.
            is_registered = true;
    return is_registered;
}

ResultCode AppletManager::PrepareToStartLibraryApplet(AppletId applet_id) {
    // The real APT service returns an error if there's a pending APT parameter when this function
    // is called.
    if (next_parameter)
        return ResultCode(ErrCodes::ParameterPresent, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    const auto& slot{applet_slots[static_cast<std::size_t>(AppletSlot::LibraryApplet)]};
    if (slot.registered)
        return ResultCode(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    if (Settings::values.use_lle_applets) {
        auto process{NS::Launch(system, FS::MediaType::NAND, GetProgramIDForApplet(applet_id))};
        if (process)
            return RESULT_SUCCESS;
        ASSERT_MSG(false, "Applet 0x{:016X} not found, dump and install it",
                   GetProgramIDForApplet(applet_id));
    }
    // Use HLE applet
    auto itr{hle_applets.find(applet_id)};
    if (itr != hle_applets.end())
        return RESULT_SUCCESS;
    else
        ASSERT_MSG(false, "Applet 0x{:03X} unimplemented", static_cast<u32>(applet_id));
}

ResultCode AppletManager::PreloadLibraryApplet(AppletId applet_id) {
    const auto& slot{applet_slots[static_cast<std::size_t>(AppletSlot::LibraryApplet)]};
    if (slot.registered)
        return ResultCode(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    if (Settings::values.use_lle_applets) {
        auto process{NS::Launch(system, FS::MediaType::NAND, GetProgramIDForApplet(applet_id))};
        if (process)
            return RESULT_SUCCESS;
        ASSERT_MSG(false, "Applet 0x{:016X} not found, dump and install it",
                   GetProgramIDForApplet(applet_id));
    }
    // Use HLE applet
    auto itr{hle_applets.find(applet_id)};
    if (itr != hle_applets.end())
        return RESULT_SUCCESS;
    else
        ASSERT_MSG(false, "Applet {:03X} unimplemented", static_cast<u32>(applet_id));
}

ResultCode AppletManager::FinishPreloadingLibraryApplet(AppletId applet_id) {
    // TODO: This function should fail depending on the applet preparation state.
    auto& slot{applet_slots[static_cast<std::size_t>(AppletSlot::LibraryApplet)]};
    slot.loaded = true;
    return RESULT_SUCCESS;
}

ResultCode AppletManager::StartLibraryApplet(AppletId applet_id,
                                             Kernel::SharedPtr<Kernel::Object> object,
                                             const std::vector<u8>& buffer) {
    MessageParameter param;
    param.destination_id = applet_id;
    param.sender_id = AppletId::Program;
    param.object = object;
    param.signal = SignalType::Wakeup;
    param.buffer = buffer;
    CancelAndSendParameter(param);
    // In case the applet is being HLEd, attempt to communicate with it.
    if (!Settings::values.use_lle_applets) {
        AppletStartupParameter parameter;
        parameter.object = object;
        parameter.buffer = buffer;
        return hle_applets.find(applet_id)->second->Start(parameter);
    } else
        return RESULT_SUCCESS;
}

ResultVal<AppletManager::AppletInfo> AppletManager::GetAppletInfo(AppletId program_id) {
    const auto slot{GetAppletSlotData(program_id)};
    if (!slot || !slot->registered) {
        // See if using HLE applets and there's an HLE applet and try to use it before erroring out.
        if (!Settings::values.use_lle_applets && hle_applets.find(program_id) == hle_applets.end())
            return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet,
                              ErrorSummary::NotFound, ErrorLevel::Status);
        LOG_WARNING(Service_APT, "Using HLE applet info for applet {:03X}",
                    static_cast<u32>(program_id));
        // TODO: Get the program ID for the current applet and write it in the response[2-3]
        return MakeResult<AppletInfo>({0, FS::MediaType::NAND, true, true, 0});
    }
    if (program_id == AppletId::Program) {
        // TODO: Implement this
        LOG_ERROR(Service_APT, "Unimplemented GetAppletInfo(Program)");
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                          ErrorLevel::Status);
    }
    return MakeResult<AppletInfo>({GetProgramIDForApplet(program_id), FS::MediaType::NAND,
                                   slot->registered, slot->loaded, slot->attributes.raw});
}

void AppletManager::ScheduleEvent(AppletId id) {
    system.CoreTiming().ScheduleEvent(usToCycles(applet_update_interval_us), applet_update_event,
                                      static_cast<u64>(id));
}

ResultCode AppletManager::PrepareToCloseLibraryApplet(bool not_pause, bool exiting,
                                                      bool jump_to_home) {
    if (next_parameter)
        return ResultCode(ErrCodes::ParameterPresent, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    if (!not_pause)
        library_applet_closing_command = SignalType::WakeupByPause;
    else if (jump_to_home)
        library_applet_closing_command = SignalType::WakeupToJumpHome;
    else if (exiting)
        library_applet_closing_command = SignalType::WakeupByCancel;
    else
        library_applet_closing_command = SignalType::WakeupByExit;
    return RESULT_SUCCESS;
}

ResultCode AppletManager::CloseLibraryApplet(Kernel::SharedPtr<Kernel::Object> object,
                                             std::vector<u8> buffer) {
    auto& slot{applet_slots[static_cast<std::size_t>(AppletSlot::LibraryApplet)]};
    MessageParameter param;
    // TODO: The destination id should be the "current applet slot id", which changes
    // constantly depending on what is going on in the system. Most of the time it is the running
    // program, but it could be something else if a system applet is launched.
    param.destination_id = AppletId::Program;
    param.sender_id = slot.applet_id;
    param.object = std::move(object);
    param.signal = library_applet_closing_command;
    param.buffer = std::move(buffer);
    auto result{SendParameter(param)};
    if (library_applet_closing_command != SignalType::WakeupByPause)
        // TODO: Terminate the running applet title
        slot.Reset();
    return result;
}

AppletManager::AppletManager(Core::System& system) : system{system} {
    auto& kernel{system.Kernel()};
    for (std::size_t slot{}; slot < applet_slots.size(); ++slot) {
        auto& slot_data{applet_slots[slot]};
        slot_data.slot = static_cast<AppletSlot>(slot);
        slot_data.applet_id = AppletId::None;
        slot_data.attributes.raw = 0;
        slot_data.registered = false;
        slot_data.loaded = false;
        slot_data.notification_event =
            kernel.CreateEvent(Kernel::ResetType::OneShot, "APT Notification");
        slot_data.parameter_event = kernel.CreateEvent(Kernel::ResetType::OneShot, "APT Parameter");
    }
    applet_update_event = system.CoreTiming().RegisterEvent(
        "HLE Applet Update Event",
        [this](u64 userdata, s64 cycles_late) { AppletUpdateEvent(userdata, cycles_late); });
    using namespace HLE::Applets;
    hle_applets.emplace(AppletId::SoftwareKeyboard1,
                        std::make_shared<SoftwareKeyboard>(AppletId::SoftwareKeyboard1, *this));
    hle_applets.emplace(AppletId::SoftwareKeyboard2,
                        std::make_shared<SoftwareKeyboard>(AppletId::SoftwareKeyboard2, *this));
    hle_applets.emplace(AppletId::Ed1, std::make_shared<MiiSelector>(AppletId::Ed1, *this));
    hle_applets.emplace(AppletId::Ed2, std::make_shared<MiiSelector>(AppletId::Ed2, *this));
    hle_applets.emplace(AppletId::Error, std::make_shared<MiiSelector>(AppletId::Error, *this));
    hle_applets.emplace(AppletId::Error2, std::make_shared<MiiSelector>(AppletId::Error2, *this));
    hle_applets.emplace(AppletId::Mint, std::make_shared<MiiSelector>(AppletId::Mint, *this));
    hle_applets.emplace(AppletId::Mint2, std::make_shared<MiiSelector>(AppletId::Mint2, *this));
}

AppletManager::~AppletManager() {
    system.CoreTiming().RemoveEvent(applet_update_event);
}

} // namespace Service::APT
