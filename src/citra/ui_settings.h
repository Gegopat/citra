// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>
#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include "common/common_types.h"

namespace UISettings {

using ContextualShortcut = std::pair<QString, int>;
using Shortcut = std::pair<QString, ContextualShortcut>;

using Themes = std::array<std::pair<const char*, const char*>, 4>;
extern const Themes themes;
extern const QStringList resolutions;

struct AppDir {
    QString path;
    bool deep_scan;
    bool expanded;

    bool operator==(const AppDir& rhs) const {
        return path == rhs.path;
    }

    bool operator!=(const AppDir& rhs) const {
        return !operator==(rhs);
    }
};

enum class ProgramListIconSize {
    NoIcon,    ///< Don't display icons
    SmallIcon, ///< Display a small (24x24) icon
    LargeIcon, ///< Display a large (48x48) icon
};

enum class ProgramListText {
    NoText = -1, ///< No text
    FileName,    ///< Display the file name of the entry
    FullPath,    ///< Display the full path of the entry
    ProgramName, ///< Display the name of the program
    ProgramID,   ///< Display the program ID
    Publisher,   ///< Display the publisher
};

struct Values {
    QByteArray geometry;
    QByteArray state;
    QByteArray screens_geometry;
    QByteArray programlist_header_state;
    QByteArray configuration_geometry;

    bool single_window_mode;
    bool fullscreen;
    bool show_filter_bar;
    bool show_status_bar;

    QString amiibo_dir;
    QString programs_dir;
    QString movies_dir;
    QString ram_dumps_dir;
    QString screenshots_dir;
    QString seeds_dir;

    // Program list
    ProgramListIconSize program_list_icon_size;
    ProgramListText program_list_row_1;
    ProgramListText program_list_row_2;
    bool program_list_hide_no_icon;

    QList<UISettings::AppDir> program_dirs;
    QStringList recent_files;

    bool confirm_close;
    bool enable_discord_rpc;

    QString theme;

    // Shortcut name <Shortcut, context>
    std::vector<Shortcut> shortcuts;

    // Multiplayer settings
    QString nickname;
    QString ip;
    QString port;
    QString room_nickname;
    QString room_name;
    quint32 max_members;
    QString room_port;
    uint host_type;
    QString room_description;
    std::vector<std::string> ban_list;

    // Logging
    bool show_console;
};

extern Values values;

} // namespace UISettings

Q_DECLARE_METATYPE(UISettings::AppDir*);
