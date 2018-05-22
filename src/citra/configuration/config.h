// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>
#include "core/settings.h"

namespace Core {
class System;
} // namespace Core

class QSettings;

class Config {
public:
    explicit Config(Core::System& system);
    ~Config();

    void LogErrors();
    void Save();
    void RestoreDefaults();
    QVariant ReadSetting(const QString& name);
    QVariant ReadSetting(const QString& name, const QVariant& default_value);
    void WriteSetting(const QString& name, const QVariant& value);
    void WriteSetting(const QString& name, const QVariant& value, const QVariant& default_value);

    static const std::array<int, Settings::NativeButton::NumButtons> default_buttons;
    static const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> default_analogs;

private:
    void Load();

    std::unique_ptr<QSettings> settings;
    std::vector<std::string> errors;
    Core::System& system;
};
