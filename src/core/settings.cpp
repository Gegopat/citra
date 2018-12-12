// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "audio_core/hle/hle.h"
#include "core/core.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/mic/mic_u.h"
#include "core/settings.h"
#include "video_core/renderer/renderer.h"
#include "video_core/video_core.h"

namespace Settings {

Values values;

void Apply(Core::System& system) {
    if (!system.IsPoweredOn())
        return;
    VideoCore::g_hw_shaders_enabled = values.use_hw_shaders;
    VideoCore::g_hw_shaders_accurate_gs = values.shaders_accurate_gs;
    VideoCore::g_hw_shaders_accurate_mul = values.shaders_accurate_mul;
    VideoCore::g_bg_color_update_requested = true;
    VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
    auto& dsp{system.DSP()};
    dsp.UpdateSink();
    dsp.EnableStretching(values.enable_audio_stretching);
    auto& sm{system.ServiceManager()};
    auto hid{sm.GetService<Service::HID::Module::Interface>("hid:USER")->GetModule()};
    hid->ReloadInputDevices();
    auto ir_user{sm.GetService<Service::IR::IR_USER>("ir:USER")};
    auto ir_rst{sm.GetService<Service::IR::IR_RST>("ir:rst")};
    ir_user->ReloadInputDevices();
    ir_rst->ReloadInputDevices();
    auto cam{sm.GetService<Service::CAM::Module::Interface>("cam:u")->GetModule()};
    cam->ReloadCameraDevices();
}

template <typename T>
void LogSetting(const std::string& name, const T& value) {
    LOG_INFO(Config, "{}: {}", name, value);
}

void LogSettings() {
    LOG_INFO(Config, "Citra Configuration:");
    LogSetting("ControlPanel_Volume", values.volume);
    LogSetting("ControlPanel_Factor3d", values.factor_3d);
    LogSetting("ControlPanel_HeadphonesConnected", values.headphones_connected);
    LogSetting("ControlPanel_AdapterConnected", values.p_adapter_connected);
    LogSetting("ControlPanel_BatteryCharging", values.p_battery_charging);
    LogSetting("ControlPanel_BatteryLevel", values.p_battery_level);
    LogSetting("ControlPanel_NetworkState", values.n_state);
    LogSetting("ControlPanel_WifiLinkLevel", values.n_wifi_link_level);
    LogSetting("ControlPanel_WifiStatus", values.n_wifi_status);
    LogSetting("Core_KeyboardMode", static_cast<int>(values.keyboard_mode));
    LogSetting("Core_EnableNsLaunch", values.enable_ns_launch);
    LogSetting("Graphics_EnableShadows", values.enable_shadows);
    LogSetting("Graphics_UseFrameLimit", values.use_frame_limit);
    LogSetting("Graphics_FrameLimit", values.frame_limit);
    LogSetting("Graphics_ScreenRefreshRate", values.screen_refresh_rate);
    LogSetting("Graphics_MinVerticesPerThread", values.min_vertices_per_thread);
    LogSetting("Graphics_ResolutionFactor", values.resolution_factor);
    LogSetting("Graphics_UseHwShaders", values.use_hw_shaders);
    LogSetting("Graphics_ShadersAccurateGs", values.shaders_accurate_gs);
    LogSetting("Graphics_ShadersAccurateMul", values.shaders_accurate_mul);
    LogSetting("Graphics_EnableCacheClear", values.enable_cache_clear);
    LogSetting("Layout_LayoutOption", static_cast<int>(values.layout_option));
    LogSetting("Layout_SwapScreens", values.swap_screens);
    bool using_lle_modules{};
    for (const auto& module : values.lle_modules)
        if (module.second) {
            using_lle_modules = true;
            break;
        }
    LogSetting("Lle_UsingLleModules", using_lle_modules);
    LogSetting("Lle_UseLleApplets", values.use_lle_applets);
    LogSetting("Lle_UseLleDsp", values.use_lle_dsp);
    LogSetting("Audio_EnableAudioStretching", values.enable_audio_stretching);
    LogSetting("Audio_OutputDevice", values.output_device);
    using namespace Service::CAM;
    LogSetting("Camera_OuterRightName", values.camera_name[OuterRightCamera]);
    LogSetting("Camera_OuterRightConfig", values.camera_config[OuterRightCamera]);
    LogSetting("Camera_OuterRightFlip", values.camera_flip[OuterRightCamera]);
    LogSetting("Camera_InnerName", values.camera_name[InnerCamera]);
    LogSetting("Camera_InnerConfig", values.camera_config[InnerCamera]);
    LogSetting("Camera_InnerFlip", values.camera_flip[InnerCamera]);
    LogSetting("Camera_OuterLeftName", values.camera_name[OuterLeftCamera]);
    LogSetting("Camera_OuterLeftConfig", values.camera_config[OuterLeftCamera]);
    LogSetting("Camera_OuterLeftFlip", values.camera_flip[OuterLeftCamera]);
    LogSetting("DataStorage_UseVirtualSd", values.use_virtual_sd);
    LogSetting("System_RegionValue", values.region_value);
    LogSetting("Hacks_PriorityBoost", values.priority_boost);
    LogSetting("Hacks_Ticks", values.ticks);
    LogSetting("Hacks_TicksMode", static_cast<int>(values.ticks_mode));
    LogSetting("Hacks_IgnoreFormatReinterpretation", values.ignore_format_reinterpretation);
    LogSetting("Hacks_DisableMh2xMsaa", values.disable_mh_2xmsaa);
}

void LoadProfile(int index) {
    const auto& profile{values.profiles.at(index)};
    values.profile = index;
    values.analogs = profile.analogs;
    values.buttons = profile.buttons;
    values.motion_device = profile.motion_device;
    values.touch_device = profile.touch_device;
    values.udp_input_address = profile.udp_input_address;
    values.udp_input_port = profile.udp_input_port;
    values.udp_pad_index = profile.udp_pad_index;
}

void SaveProfile(int index) {
    auto& profile{values.profiles.at(index)};
    profile.analogs = values.analogs;
    profile.buttons = values.buttons;
    profile.motion_device = values.motion_device;
    profile.touch_device = values.touch_device;
    profile.udp_input_address = values.udp_input_address;
    profile.udp_input_port = values.udp_input_port;
    profile.udp_pad_index = values.udp_pad_index;
}

void CreateProfile(std::string name) {
    ControllerProfile profile{};
    profile.name = std::move(name);
    profile.analogs = values.analogs;
    profile.buttons = values.buttons;
    profile.motion_device = values.motion_device;
    profile.touch_device = values.touch_device;
    profile.udp_input_address = values.udp_input_address;
    profile.udp_input_port = values.udp_input_port;
    profile.udp_pad_index = values.udp_pad_index;
    values.profiles.push_back(profile);
    LoadProfile(values.profiles.size() - 1);
}

void DeleteProfile(int index) {
    values.profiles.erase(values.profiles.begin() + index);
    LoadProfile(0);
}

void RenameCurrentProfile(std::string new_name) {
    values.profiles.at(values.profile).name = std::move(new_name);
}

} // namespace Settings
