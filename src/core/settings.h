// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "core/hle/service/cam/cam.h"

namespace Core {
class System;
} // namespace Core

namespace Settings {

enum class KeyboardMode { StdIn, Qt };

enum class TicksMode { Auto, Accurate, Custom };

enum class InitClock {
    SystemTime = 0,
    FixedTime = 1,
};

enum class LayoutOption {
    Default,
    SingleScreen,
    MediumScreen,
    LargeScreen,
    SideScreen,
};

namespace NativeButton {
enum Values {
    A,
    B,
    X,
    Y,
    Up,
    Down,
    Left,
    Right,
    L,
    R,
    Start,
    Select,

    ZL,
    ZR,

    Home,

    NumButtons,
};

constexpr int BUTTON_HID_BEGIN{A};
constexpr int BUTTON_IR_BEGIN{ZL};
constexpr int BUTTON_NS_BEGIN{Home};

constexpr int BUTTON_HID_END{BUTTON_IR_BEGIN};
constexpr int BUTTON_IR_END{BUTTON_NS_BEGIN};
constexpr int BUTTON_NS_END{NumButtons};

constexpr int NUM_BUTTONS_HID{BUTTON_HID_END - BUTTON_HID_BEGIN};
constexpr int NUM_BUTTONS_IR{BUTTON_IR_END - BUTTON_IR_BEGIN};
constexpr int NUM_BUTTONS_NS{BUTTON_NS_END - BUTTON_NS_BEGIN};

static const std::array<const char*, NumButtons> mapping{{
    "button_a",
    "button_b",
    "button_x",
    "button_y",
    "button_up",
    "button_down",
    "button_left",
    "button_right",
    "button_l",
    "button_r",
    "button_start",
    "button_select",
    "button_zl",
    "button_zr",
    "button_home",
}};
} // namespace NativeButton

namespace NativeAnalog {
enum Values {
    CirclePad,
    CStick,

    NumAnalogs,
};

static const std::array<const char*, NumAnalogs> mapping{{
    "circle_pad",
    "c_stick",
}};
} // namespace NativeAnalog

struct ControllerProfile {
    std::string name;
    std::array<std::string, NativeButton::NumButtons> buttons;
    std::array<std::string, NativeAnalog::NumAnalogs> analogs;
    std::string motion_device;
    std::string touch_device;
    std::string udp_input_address;
    u16 udp_input_port;
    u8 udp_pad_index;
};

struct Values {
    // Control Panel
    float volume;
    bool headphones_connected;
    u8 factor_3d;
    bool p_adapter_connected;
    bool p_battery_charging;
    u32 p_battery_level;
    u32 n_wifi_status;
    u8 n_wifi_link_level;
    u8 n_state;

    // Controls
    std::array<std::string, NativeButton::NumButtons> buttons;
    std::array<std::string, NativeAnalog::NumAnalogs> analogs;
    std::string motion_device;
    std::string touch_device;
    std::string udp_input_address;
    u16 udp_input_port;
    u8 udp_pad_index;
    int profile;
    std::vector<ControllerProfile> profiles;

    // Core
    KeyboardMode keyboard_mode;
    bool enable_ns_launch;

    // LLE
    std::unordered_map<std::string, bool> lle_modules;
    bool use_lle_applets;

    // Data Storage
    bool use_virtual_sd;
    std::string nand_dir;
    std::string sdmc_dir;

    // System
    int region_value;
    InitClock init_clock;
    u64 init_time;

    // Graphics
    bool use_hw_shaders;
    bool shaders_accurate_gs;
    bool shaders_accurate_mul;
    u16 resolution_factor;
    bool use_frame_limit;
    u16 frame_limit;
    bool enable_shadows;
    int screen_refresh_rate;
    int min_vertices_per_thread;
    bool enable_cache_clear;

    LayoutOption layout_option;
    bool swap_screens;
    bool custom_layout;
    u16 custom_top_left;
    u16 custom_top_top;
    u16 custom_top_right;
    u16 custom_top_bottom;
    u16 custom_bottom_left;
    u16 custom_bottom_top;
    u16 custom_bottom_right;
    u16 custom_bottom_bottom;

    float bg_red;
    float bg_green;
    float bg_blue;

    // Logging
    std::string log_filter;

    // Audio
    bool enable_audio_stretching;
    std::string output_device;

    // Camera
    std::array<std::string, Service::CAM::NumCameras> camera_name;
    std::array<std::string, Service::CAM::NumCameras> camera_config;
    std::array<int, Service::CAM::NumCameras> camera_flip;

    // Hacks
    bool priority_boost;
    TicksMode ticks_mode;
    u64 ticks;
    bool ignore_format_reinterpretation;
    bool disable_mh_2xmsaa;
    bool force_memory_mode_7;
} extern values;

// A special value for Values::region_value indicating that citra will automatically select a region
// value to fit the region lockout info of the program
constexpr int REGION_VALUE_AUTO_SELECT{-1};

void Apply(Core::System& system);
void LogSettings();

// Controller profiles
void LoadProfile(int index);
void SaveProfile(int index);
void CreateProfile(std::string name);
void DeleteProfile(int index);
void RenameCurrentProfile(std::string new_name);

} // namespace Settings
