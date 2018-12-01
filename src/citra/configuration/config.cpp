// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QSettings>
#include "citra/configuration/config.h"
#include "citra/ui_settings.h"
#include "common/file_util.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"
#include "network/room.h"

const std::array<int, Settings::NativeButton::NumButtons> Config::default_buttons{{
    Qt::Key_A,
    Qt::Key_S,
    Qt::Key_Z,
    Qt::Key_X,
    Qt::Key_T,
    Qt::Key_G,
    Qt::Key_F,
    Qt::Key_H,
    Qt::Key_Q,
    Qt::Key_W,
    Qt::Key_M,
    Qt::Key_N,
    Qt::Key_1,
    Qt::Key_2,
    Qt::Key_B,
}};

const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> Config::default_analogs{{
    {
        Qt::Key_Up,
        Qt::Key_Down,
        Qt::Key_Left,
        Qt::Key_Right,
        Qt::Key_D,
    },
    {
        Qt::Key_I,
        Qt::Key_K,
        Qt::Key_J,
        Qt::Key_L,
        Qt::Key_D,
    },
}};

Config::Config(Core::System& system) : system{system} {
    std::string path{FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "qt-config.ini"};
    FileUtil::CreateFullPath(path);
    qt_config = std::make_unique<QSettings>(QString::fromStdString(path), QSettings::IniFormat);
    Load();
}

Config::~Config() {
    Save();
}

void Config::Load() {
    qt_config->beginGroup("ControlPanel");
    Settings::values.volume = qt_config->value("volume", 1).toFloat();
    Settings::values.headphones_connected =
        qt_config->value("headphones_connected", false).toBool();
    Settings::values.factor_3d = qt_config->value("factor_3d", 0).toInt();
    Settings::values.p_adapter_connected = qt_config->value("p_adapter_connected", true).toBool();
    Settings::values.p_battery_charging = qt_config->value("p_battery_charging", true).toBool();
    Settings::values.p_battery_level =
        static_cast<u32>(qt_config->value("p_battery_level", 5).toInt());
    Settings::values.n_wifi_status = static_cast<u32>(qt_config->value("n_wifi_status", 0).toInt());
    Settings::values.n_wifi_link_level =
        static_cast<u8>(qt_config->value("n_wifi_link_level", 0).toInt());
    Settings::values.n_state = static_cast<u8>(qt_config->value("n_state", 0).toInt());
    qt_config->endGroup();
    qt_config->beginGroup("Controls");
    int size{qt_config->beginReadArray("profiles")};
    for (int p{}; p < size; ++p) {
        qt_config->setArrayIndex(p);
        Settings::ControllerProfile profile;
        profile.name = qt_config->value("name", "default").toString().toStdString();
        for (int i{}; i < Settings::NativeButton::NumButtons; ++i) {
            std::string default_param{InputCommon::GenerateKeyboardParam(default_buttons[i])};
            profile.buttons[i] = qt_config
                                     ->value(Settings::NativeButton::mapping[i],
                                             QString::fromStdString(default_param))
                                     .toString()
                                     .toStdString();
            if (profile.buttons[i].empty())
                profile.buttons[i] = default_param;
        }
        for (int i{}; i < Settings::NativeAnalog::NumAnalogs; ++i) {
            std::string default_param{InputCommon::GenerateAnalogParamFromKeys(
                default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
                default_analogs[i][3], default_analogs[i][4], 0.5f)};
            profile.analogs[i] = qt_config
                                     ->value(Settings::NativeAnalog::mapping[i],
                                             QString::fromStdString(default_param))
                                     .toString()
                                     .toStdString();
            if (profile.analogs[i].empty())
                profile.analogs[i] = default_param;
        }
        profile.motion_device =
            qt_config
                ->value("motion_device",
                        "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0")
                .toString()
                .toStdString();
        profile.touch_device =
            qt_config->value("touch_device", "engine:emu_window").toString().toStdString();
        profile.udp_input_address =
            qt_config->value("udp_input_address", InputCommon::CemuhookUDP::DEFAULT_ADDR)
                .toString()
                .toStdString();
        profile.udp_input_port = static_cast<u16>(
            qt_config->value("udp_input_port", InputCommon::CemuhookUDP::DEFAULT_PORT).toInt());
        profile.udp_pad_index = static_cast<u8>(qt_config->value("udp_pad_index", 0).toUInt());
        Settings::values.profiles.push_back(profile);
    }
    qt_config->endArray();
    Settings::values.profile = qt_config->value("profile", 0).toInt();
    if (size == 0) {
        Settings::ControllerProfile profile{};
        profile.name = "default";
        for (int i{}; i < Settings::NativeButton::NumButtons; ++i) {
            std::string default_param{InputCommon::GenerateKeyboardParam(default_buttons[i])};
            profile.buttons[i] = qt_config
                                     ->value(Settings::NativeButton::mapping[i],
                                             QString::fromStdString(default_param))
                                     .toString()
                                     .toStdString();
            if (profile.buttons[i].empty())
                profile.buttons[i] = default_param;
        }
        for (int i{}; i < Settings::NativeAnalog::NumAnalogs; ++i) {
            std::string default_param{InputCommon::GenerateAnalogParamFromKeys(
                default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
                default_analogs[i][3], default_analogs[i][4], 0.5f)};
            profile.analogs[i] = qt_config
                                     ->value(Settings::NativeAnalog::mapping[i],
                                             QString::fromStdString(default_param))
                                     .toString()
                                     .toStdString();
            if (profile.analogs[i].empty())
                profile.analogs[i] = default_param;
        }
        profile.motion_device =
            qt_config
                ->value("motion_device",
                        "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0")
                .toString()
                .toStdString();
        profile.touch_device =
            qt_config->value("touch_device", "engine:emu_window").toString().toStdString();
        profile.udp_input_address =
            qt_config->value("udp_input_address", InputCommon::CemuhookUDP::DEFAULT_ADDR)
                .toString()
                .toStdString();
        profile.udp_input_port = static_cast<u16>(
            qt_config->value("udp_input_port", InputCommon::CemuhookUDP::DEFAULT_PORT).toInt());
        profile.udp_pad_index = static_cast<u8>(qt_config->value("udp_pad_index", 0).toUInt());
        Settings::values.profiles.push_back(profile);
    }
    if (Settings::values.profile >= size) {
        Settings::values.profile = 0;
        errors.push_back("Invalid profile index");
    }
    Settings::LoadProfile(Settings::values.profile);
    qt_config->endGroup();
    qt_config->beginGroup("Core");
    Settings::values.keyboard_mode =
        static_cast<Settings::KeyboardMode>(qt_config->value("keyboard_mode", 1).toInt());
    Settings::values.enable_ns_launch = qt_config->value("enable_ns_launch", false).toBool();
    qt_config->endGroup();
    qt_config->beginGroup("LLE");
    for (const auto& service_module : Service::service_module_map) {
        bool use_lle{qt_config->value(QString::fromStdString(service_module.name), false).toBool()};
        Settings::values.lle_modules.emplace(service_module.name, use_lle);
    }
    Settings::values.use_lle_applets = qt_config->value("use_lle_applets", false).toBool();
    qt_config->endGroup();
    qt_config->beginGroup("Graphics");
    Settings::values.enable_shadows = qt_config->value("enable_shadows", true).toBool();
    Settings::values.use_frame_limit = qt_config->value("use_frame_limit", true).toBool();
    Settings::values.frame_limit = qt_config->value("frame_limit", 100).toInt();
    Settings::values.screen_refresh_rate = qt_config->value("screen_refresh_rate", 60).toInt();
    Settings::values.min_vertices_per_thread =
        qt_config->value("min_vertices_per_thread", 10).toInt();
    u16 resolution_factor{static_cast<u16>(qt_config->value("resolution_factor", 1).toInt())};
    if (resolution_factor == 0)
        resolution_factor = 1;
    Settings::values.resolution_factor = resolution_factor;
#ifdef __APPLE__
    // Hardware shaders are broken on macOS thanks to poor drivers.
    // We still want to provide this option for test/development purposes, but disable it by
    // default.
    Settings::values.use_hw_shaders = qt_config->value("use_hw_shaders", false).toBool();
#else
    Settings::values.use_hw_shaders = qt_config->value("use_hw_shaders", true).toBool();
#endif
    Settings::values.shaders_accurate_gs = qt_config->value("shaders_accurate_gs", true).toBool();
    Settings::values.shaders_accurate_mul =
        qt_config->value("shaders_accurate_mul", false).toBool();
    Settings::values.bg_red = qt_config->value("bg_red", 0.0).toFloat();
    Settings::values.bg_green = qt_config->value("bg_green", 0.0).toFloat();
    Settings::values.bg_blue = qt_config->value("bg_blue", 0.0).toFloat();
    Settings::values.enable_cache_clear = qt_config->value("enable_cache_clear", false).toBool();
    qt_config->endGroup();
    qt_config->beginGroup("Layout");
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(qt_config->value("layout_option").toInt());
    Settings::values.swap_screens = qt_config->value("swap_screens", false).toBool();
    Settings::values.custom_layout = qt_config->value("custom_layout", false).toBool();
    Settings::values.custom_top_left = qt_config->value("custom_top_left", 0).toInt();
    Settings::values.custom_top_top = qt_config->value("custom_top_top", 0).toInt();
    Settings::values.custom_top_right = qt_config->value("custom_top_right", 400).toInt();
    Settings::values.custom_top_bottom = qt_config->value("custom_top_bottom", 240).toInt();
    Settings::values.custom_bottom_left = qt_config->value("custom_bottom_left", 40).toInt();
    Settings::values.custom_bottom_top = qt_config->value("custom_bottom_top", 240).toInt();
    Settings::values.custom_bottom_right = qt_config->value("custom_bottom_right", 360).toInt();
    Settings::values.custom_bottom_bottom = qt_config->value("custom_bottom_bottom", 480).toInt();
    qt_config->endGroup();
    qt_config->beginGroup("Audio");
    Settings::values.enable_audio_stretching =
        qt_config->value("enable_audio_stretching", true).toBool();
    Settings::values.output_device =
        qt_config->value("output_device", "auto").toString().toStdString();
    qt_config->endGroup();
    using namespace Service::CAM;
    qt_config->beginGroup("Camera");
    Settings::values.camera_name[OuterRightCamera] =
        qt_config->value("camera_outer_right_name", "blank").toString().toStdString();
    Settings::values.camera_config[OuterRightCamera] =
        qt_config->value("camera_outer_right_config", "").toString().toStdString();
    Settings::values.camera_flip[OuterRightCamera] =
        qt_config->value("camera_outer_right_flip", 0).toInt();
    Settings::values.camera_name[InnerCamera] =
        qt_config->value("camera_inner_name", "blank").toString().toStdString();
    Settings::values.camera_config[InnerCamera] =
        qt_config->value("camera_inner_config", "").toString().toStdString();
    Settings::values.camera_flip[InnerCamera] = qt_config->value("camera_inner_flip", 0).toInt();
    Settings::values.camera_name[OuterLeftCamera] =
        qt_config->value("camera_outer_left_name", "blank").toString().toStdString();
    Settings::values.camera_config[OuterLeftCamera] =
        qt_config->value("camera_outer_left_config", "").toString().toStdString();
    Settings::values.camera_flip[OuterLeftCamera] =
        qt_config->value("camera_outer_left_flip", 0).toInt();
    qt_config->endGroup();
    qt_config->beginGroup("Data Storage");
    Settings::values.use_virtual_sd = qt_config->value("use_virtual_sd", true).toBool();
    Settings::values.nand_dir = qt_config->value("nand_dir", "").toString().toStdString();
    Settings::values.sdmc_dir = qt_config->value("sdmc_dir", "").toString().toStdString();
    qt_config->endGroup();
    qt_config->beginGroup("System");
    Settings::values.region_value =
        qt_config->value("region_value", Settings::REGION_VALUE_AUTO_SELECT).toInt();
    Settings::values.init_clock = static_cast<Settings::InitClock>(
        qt_config->value("init_clock", static_cast<u32>(Settings::InitClock::SystemTime)).toInt());
    Settings::values.init_time = qt_config->value("init_time", 946681277ULL).toULongLong();
    qt_config->endGroup();
    qt_config->beginGroup("Miscellaneous");
    Settings::values.log_filter = qt_config->value("log_filter", "*:Info").toString().toStdString();
    qt_config->endGroup();
    qt_config->beginGroup("Hacks");
    Settings::values.priority_boost = qt_config->value("priority_boost", false).toBool();
    Settings::values.ticks_mode =
        static_cast<Settings::TicksMode>(qt_config->value("ticks_mode", 0).toInt());
    Settings::values.ticks = qt_config->value("ticks", 0).toULongLong();
    Settings::values.ignore_format_reinterpretation =
        qt_config->value("ignore_format_reinterpretation", false).toBool();
    Settings::values.force_memory_mode_7 = qt_config->value("force_memory_mode_7", false).toBool();
    Settings::values.disable_mh_2xmsaa = qt_config->value("disable_mh_2xmsaa", false).toBool();
    qt_config->endGroup();
    qt_config->beginGroup("UI");
    UISettings::values.confirm_close = qt_config->value("confirm_close", true).toBool();
    UISettings::values.theme = qt_config->value("theme", UISettings::themes[0].second).toString();
    qt_config->beginGroup("UILayout");
    UISettings::values.geometry = qt_config->value("geometry").toByteArray();
    UISettings::values.state = qt_config->value("state").toByteArray();
    UISettings::values.screens_geometry = qt_config->value("geometryScreens").toByteArray();
    UISettings::values.programlist_header_state =
        qt_config->value("programListHeaderState").toByteArray();
    UISettings::values.configuration_geometry =
        qt_config->value("configurationGeometry").toByteArray();
    qt_config->endGroup();
    qt_config->beginGroup("ProgramList");
    int icon_size{qt_config->value("iconSize", 2).toInt()};
    if (icon_size < 0 || icon_size > 2)
        icon_size = 2;
    UISettings::values.program_list_icon_size = UISettings::ProgramListIconSize{icon_size};
    int row_1{qt_config->value("row1", 2).toInt()};
    if (row_1 < 0 || row_1 > 4)
        row_1 = 2;
    UISettings::values.program_list_row_1 = UISettings::ProgramListText{row_1};
    int row_2{qt_config->value("row2", 0).toInt()};
    if (row_2 < -1 || row_2 > 4)
        row_2 = 0;
    UISettings::values.program_list_row_2 = UISettings::ProgramListText{row_2};
    UISettings::values.program_list_hide_no_icon = qt_config->value("hideNoIcon", false).toBool();
    qt_config->endGroup();
    qt_config->beginGroup("Paths");
    UISettings::values.amiibo_dir = qt_config->value("amiibo_dir", ".").toString();
    UISettings::values.programs_dir = qt_config->value("programs_dir", ".").toString();
    UISettings::values.movies_dir = qt_config->value("movies_dir", ".").toString();
    UISettings::values.ram_dumps_dir = qt_config->value("ram_dumps_dir", ".").toString();
    UISettings::values.screenshots_dir = qt_config->value("screenshots_dir", ".").toString();
    UISettings::values.seeds_dir = qt_config->value("seeds_dir", ".").toString();
    size = qt_config->beginReadArray("appdirs");
    for (int i{}; i < size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::AppDir program_dir;
        program_dir.path = qt_config->value("path").toString();
        program_dir.deep_scan = qt_config->value("deep_scan", false).toBool();
        program_dir.expanded = qt_config->value("expanded", true).toBool();
        UISettings::values.program_dirs.append(program_dir);
    }
    qt_config->endArray();
    // Create NAND and SD card directories if empty, these aren't removable through the UI
    if (UISettings::values.program_dirs.isEmpty()) {
        UISettings::AppDir program_dir{};
        program_dir.path = "INSTALLED";
        program_dir.expanded = true;
        UISettings::values.program_dirs.append(program_dir);
        program_dir.path = "SYSTEM";
        UISettings::values.program_dirs.append(program_dir);
    }
    UISettings::values.recent_files = qt_config->value("recentFiles").toStringList();
    qt_config->endGroup();
    qt_config->beginGroup("Shortcuts");
    auto groups{qt_config->childGroups()};
    for (const auto& group : groups) {
        qt_config->beginGroup(group);
        auto hotkeys{qt_config->childGroups()};
        for (const auto& hotkey : hotkeys) {
            qt_config->beginGroup(hotkey);
            UISettings::values.shortcuts.emplace_back(UISettings::Shortcut{
                group + "/" + hotkey,
                UISettings::ContextualShortcut{qt_config->value("KeySeq").toString(),
                                               qt_config->value("Context").toInt()}});
            qt_config->endGroup();
        }
        qt_config->endGroup();
    }
    qt_config->endGroup();
    UISettings::values.single_window_mode = qt_config->value("singleWindowMode", true).toBool();
    UISettings::values.fullscreen = qt_config->value("fullscreen", false).toBool();
    UISettings::values.show_filter_bar = qt_config->value("showFilterBar", true).toBool();
    UISettings::values.show_status_bar = qt_config->value("showStatusBar", true).toBool();
    UISettings::values.show_console = qt_config->value("showConsole", false).toBool();
    qt_config->beginGroup("Multiplayer");
    UISettings::values.nickname = qt_config->value("nickname", "").toString();
    UISettings::values.ip = qt_config->value("ip", "").toString();
    UISettings::values.port = qt_config->value("port", Network::DefaultRoomPort).toString();
    UISettings::values.room_nickname = qt_config->value("room_nickname", "").toString();
    UISettings::values.room_name = qt_config->value("room_name", "").toString();
    UISettings::values.room_port = qt_config->value("room_port", "24872").toString();
    bool ok{};
    UISettings::values.host_type = qt_config->value("host_type", 0).toUInt(&ok);
    if (!ok)
        UISettings::values.host_type = 0;
    UISettings::values.max_members = qt_config->value("max_members", 8).toUInt();
    UISettings::values.room_description = qt_config->value("room_description", "").toString();
    auto list{qt_config->value("ban_list", QStringList()).toStringList()};
    for (auto s : list)
        UISettings::values.ban_list.push_back(s.toStdString());
    qt_config->endGroup();
    qt_config->endGroup();
}

void Config::LogErrors() {
    for (const auto& error : errors)
        LOG_ERROR(Config, "{}", error);
    errors.clear();
}

void Config::Save() {
    qt_config->beginGroup("ControlPanel");
    qt_config->setValue("volume", Settings::values.volume);
    qt_config->setValue("headphones_connected", Settings::values.headphones_connected);
    qt_config->setValue("factor_3d", Settings::values.factor_3d);
    qt_config->setValue("p_adapter_connected", Settings::values.p_adapter_connected);
    qt_config->setValue("p_battery_charging", Settings::values.p_battery_charging);
    qt_config->setValue("p_battery_level", Settings::values.p_battery_level);
    qt_config->setValue("n_wifi_status", Settings::values.n_wifi_status);
    qt_config->setValue("n_wifi_link_level", Settings::values.n_wifi_link_level);
    qt_config->setValue("n_state", Settings::values.n_state);
    qt_config->endGroup();
    qt_config->beginGroup("Controls");
    qt_config->beginWriteArray("profiles");
    for (int p{}; p < Settings::values.profiles.size(); ++p) {
        qt_config->setArrayIndex(p);
        const auto& profile{Settings::values.profiles[p]};
        qt_config->setValue("name", QString::fromStdString(profile.name));
        for (int i{}; i < Settings::NativeButton::NumButtons; ++i) {
            qt_config->setValue(QString::fromStdString(Settings::NativeButton::mapping[i]),
                                QString::fromStdString(profile.buttons[i]));
        }
        for (int i{}; i < Settings::NativeAnalog::NumAnalogs; ++i) {
            qt_config->setValue(QString::fromStdString(Settings::NativeAnalog::mapping[i]),
                                QString::fromStdString(profile.analogs[i]));
        }
        qt_config->setValue("motion_device", QString::fromStdString(profile.motion_device));
        qt_config->setValue("touch_device", QString::fromStdString(profile.touch_device));
        qt_config->setValue("udp_input_address", QString::fromStdString(profile.udp_input_address));
        qt_config->setValue("udp_input_port", profile.udp_input_port);
        qt_config->setValue("udp_pad_index", profile.udp_pad_index);
    }
    qt_config->endArray();
    qt_config->setValue("profile", Settings::values.profile);
    qt_config->endGroup();
    qt_config->beginGroup("Core");
    qt_config->setValue("keyboard_mode", static_cast<int>(Settings::values.keyboard_mode));
    qt_config->setValue("enable_ns_launch", Settings::values.enable_ns_launch);
    qt_config->endGroup();
    qt_config->beginGroup("LLE");
    for (const auto& service_module : Settings::values.lle_modules)
        qt_config->setValue(QString::fromStdString(service_module.first), service_module.second);
    qt_config->setValue("use_lle_applets", Settings::values.use_lle_applets);
    qt_config->endGroup();
    qt_config->beginGroup("Graphics");
    qt_config->setValue("enable_shadows", Settings::values.enable_shadows);
    qt_config->setValue("use_frame_limit", Settings::values.use_frame_limit);
    qt_config->setValue("frame_limit", Settings::values.frame_limit);
    qt_config->setValue("screen_refresh_rate", Settings::values.screen_refresh_rate);
    qt_config->setValue("min_vertices_per_thread", Settings::values.min_vertices_per_thread);
    qt_config->setValue("resolution_factor", Settings::values.resolution_factor);
    qt_config->setValue("use_hw_shaders", Settings::values.use_hw_shaders);
    qt_config->setValue("shaders_accurate_gs", Settings::values.shaders_accurate_gs);
    qt_config->setValue("shaders_accurate_mul", Settings::values.shaders_accurate_mul);
    // Cast to double because Qt's written float values aren't human-readable
    qt_config->setValue("bg_red", (double)Settings::values.bg_red);
    qt_config->setValue("bg_green", (double)Settings::values.bg_green);
    qt_config->setValue("bg_blue", (double)Settings::values.bg_blue);
    qt_config->setValue("enable_cache_clear", Settings::values.enable_cache_clear);
    qt_config->endGroup();
    qt_config->beginGroup("Layout");
    qt_config->setValue("layout_option", static_cast<int>(Settings::values.layout_option));
    qt_config->setValue("swap_screens", Settings::values.swap_screens);
    qt_config->setValue("custom_layout", Settings::values.custom_layout);
    qt_config->setValue("custom_top_left", Settings::values.custom_top_left);
    qt_config->setValue("custom_top_top", Settings::values.custom_top_top);
    qt_config->setValue("custom_top_right", Settings::values.custom_top_right);
    qt_config->setValue("custom_top_bottom", Settings::values.custom_top_bottom);
    qt_config->setValue("custom_bottom_left", Settings::values.custom_bottom_left);
    qt_config->setValue("custom_bottom_top", Settings::values.custom_bottom_top);
    qt_config->setValue("custom_bottom_right", Settings::values.custom_bottom_right);
    qt_config->setValue("custom_bottom_bottom", Settings::values.custom_bottom_bottom);
    qt_config->endGroup();
    qt_config->beginGroup("Audio");
    qt_config->setValue("enable_audio_stretching", Settings::values.enable_audio_stretching);
    qt_config->setValue("output_device", QString::fromStdString(Settings::values.output_device));
    qt_config->endGroup();
    using namespace Service::CAM;
    qt_config->beginGroup("Camera");
    qt_config->setValue("camera_outer_right_name",
                        QString::fromStdString(Settings::values.camera_name[OuterRightCamera]));
    qt_config->setValue("camera_outer_right_config",
                        QString::fromStdString(Settings::values.camera_config[OuterRightCamera]));
    qt_config->setValue("camera_outer_right_flip", Settings::values.camera_flip[OuterRightCamera]);
    qt_config->setValue("camera_inner_name",
                        QString::fromStdString(Settings::values.camera_name[InnerCamera]));
    qt_config->setValue("camera_inner_config",
                        QString::fromStdString(Settings::values.camera_config[InnerCamera]));
    qt_config->setValue("camera_inner_flip", Settings::values.camera_flip[InnerCamera]);
    qt_config->setValue("camera_outer_left_name",
                        QString::fromStdString(Settings::values.camera_name[OuterLeftCamera]));
    qt_config->setValue("camera_outer_left_config",
                        QString::fromStdString(Settings::values.camera_config[OuterLeftCamera]));
    qt_config->setValue("camera_outer_left_flip", Settings::values.camera_flip[OuterLeftCamera]);
    qt_config->endGroup();
    qt_config->beginGroup("Data Storage");
    qt_config->setValue("use_virtual_sd", Settings::values.use_virtual_sd);
    qt_config->setValue("nand_dir", QString::fromStdString(Settings::values.nand_dir));
    qt_config->setValue("sdmc_dir", QString::fromStdString(Settings::values.sdmc_dir));
    qt_config->endGroup();
    qt_config->beginGroup("System");
    qt_config->setValue("region_value", Settings::values.region_value);
    qt_config->setValue("init_clock", static_cast<u32>(Settings::values.init_clock));
    qt_config->setValue("init_time", static_cast<unsigned long long>(Settings::values.init_time));
    qt_config->endGroup();
    qt_config->beginGroup("Miscellaneous");
    qt_config->setValue("log_filter", QString::fromStdString(Settings::values.log_filter));
    qt_config->endGroup();
    qt_config->beginGroup("Hacks");
    qt_config->setValue("priority_boost", Settings::values.priority_boost);
    qt_config->setValue("ticks_mode", static_cast<int>(Settings::values.ticks_mode));
    qt_config->setValue("ticks", static_cast<unsigned long long>(Settings::values.ticks));
    qt_config->setValue("ignore_format_reinterpretation",
                        Settings::values.ignore_format_reinterpretation);
    qt_config->setValue("force_memory_mode_7", Settings::values.force_memory_mode_7);
    qt_config->setValue("disable_mh_2xmsaa", Settings::values.disable_mh_2xmsaa);
    qt_config->endGroup();
    qt_config->beginGroup("UI");
    qt_config->setValue("confirm_close", UISettings::values.confirm_close);
    qt_config->setValue("theme", UISettings::values.theme);
    qt_config->beginGroup("UILayout");
    qt_config->setValue("geometry", UISettings::values.geometry);
    qt_config->setValue("state", UISettings::values.state);
    qt_config->setValue("geometryScreens", UISettings::values.screens_geometry);
    qt_config->setValue("programListHeaderState", UISettings::values.programlist_header_state);
    qt_config->setValue("configurationGeometry", UISettings::values.configuration_geometry);
    qt_config->endGroup();
    qt_config->beginGroup("ProgramList");
    qt_config->setValue("iconSize", static_cast<int>(UISettings::values.program_list_icon_size));
    qt_config->setValue("row1", static_cast<int>(UISettings::values.program_list_row_1));
    qt_config->setValue("row2", static_cast<int>(UISettings::values.program_list_row_2));
    qt_config->setValue("hideNoIcon", UISettings::values.program_list_hide_no_icon);
    qt_config->endGroup();
    qt_config->beginGroup("Paths");
    qt_config->setValue("amiibo_dir", UISettings::values.amiibo_dir);
    qt_config->setValue("programs_dir", UISettings::values.programs_dir);
    qt_config->setValue("movies_dir", UISettings::values.movies_dir);
    qt_config->setValue("ram_dumps_dir", UISettings::values.ram_dumps_dir);
    qt_config->setValue("screenshots_dir", UISettings::values.screenshots_dir);
    qt_config->setValue("seeds_dir", UISettings::values.seeds_dir);
    qt_config->beginWriteArray("appdirs");
    for (int i{}; i < UISettings::values.program_dirs.size(); ++i) {
        qt_config->setArrayIndex(i);
        const auto& program_dir{UISettings::values.program_dirs.at(i)};
        qt_config->setValue("path", program_dir.path);
        qt_config->setValue("deep_scan", program_dir.deep_scan);
        qt_config->setValue("expanded", program_dir.expanded);
    }
    qt_config->endArray();
    qt_config->setValue("recentFiles", UISettings::values.recent_files);
    qt_config->endGroup();
    qt_config->beginGroup("Shortcuts");
    for (const auto& shortcut : UISettings::values.shortcuts) {
        qt_config->setValue(shortcut.first + "/KeySeq", shortcut.second.first);
        qt_config->setValue(shortcut.first + "/Context", shortcut.second.second);
    }
    qt_config->endGroup();
    qt_config->setValue("singleWindowMode", UISettings::values.single_window_mode);
    qt_config->setValue("fullscreen", UISettings::values.fullscreen);
    qt_config->setValue("showFilterBar", UISettings::values.show_filter_bar);
    qt_config->setValue("showStatusBar", UISettings::values.show_status_bar);
    qt_config->setValue("showConsole", UISettings::values.show_console);
    qt_config->beginGroup("Multiplayer");
    qt_config->setValue("nickname", UISettings::values.nickname);
    qt_config->setValue("ip", UISettings::values.ip);
    qt_config->setValue("port", UISettings::values.port);
    qt_config->setValue("room_nickname", UISettings::values.room_nickname);
    qt_config->setValue("room_name", UISettings::values.room_name);
    qt_config->setValue("room_port", UISettings::values.room_port);
    qt_config->setValue("host_type", UISettings::values.host_type);
    qt_config->setValue("max_members", UISettings::values.max_members);
    qt_config->setValue("room_description", UISettings::values.room_description);
    QStringList list;
    for (const auto& i : UISettings::values.ban_list)
        list.append(QString::fromStdString(i));
    qt_config->setValue("ban_list", list);
    qt_config->endGroup();
    qt_config->endGroup();
}

void Config::RestoreDefaults() {
    qt_config->clear();
    Settings::values.profiles.clear();
    Load();
    Save();
    Settings::Apply(system);
}
