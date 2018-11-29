// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "ui_settings.h"

namespace UISettings {

const Themes themes{{
    {"Default", "default"},
    {"Dark", "qdarkstyle"},
    {"Colorful", "colorful"},
    {"Colorful Dark", "colorful_dark"},
}};

const QStringList resolutions{{
    "Native (400x240)",
    "2x Native (800x480)",
    "3x Native (1200x720)",
    "4x Native (1600x960)",
    "5x Native (2000x1200)",
    "6x Native (2400x1440)",
    "7x Native (2800x1680)",
    "8x Native (3200x1920)",
    "9x Native (3600x2160)",
    "10x Native (4000x2400)",
}};

Values values;

} // namespace UISettings
