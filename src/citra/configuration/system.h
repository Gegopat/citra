// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>
#include "common/common_types.h"

namespace Core {
class System;
} // namespace Core

namespace Service::CFG {
class Module;
} // namespace Service::CFG

namespace Ui {
class ConfigurationSystem;
} // namespace Ui

class ConfigurationSystem : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurationSystem(QWidget* parent = nullptr);
    ~ConfigurationSystem() override;

    void LoadConfiguration(Core::System& system);
    void ApplyConfiguration();

private:
    void ReadSystemSettings();
    void UpdateBirthdayComboBox(int birthmonth_index);
    void UpdateInitTime(int init_clock);
    void RefreshConsoleID();

    std::unique_ptr<Ui::ConfigurationSystem> ui;
    bool enabled;
    std::shared_ptr<Service::CFG::Module> cfg;
    std::u16string username;
    int birthmonth, birthday, language_index, sound_index, model_index;
    u8 country_code;
};
