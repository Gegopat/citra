// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Core {
class System;
} // namespace Core

namespace Ui {
class ConfigurationHacks;
} // namespace Ui

class ConfigurationHacks : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurationHacks(QWidget* parent = nullptr);
    ~ConfigurationHacks();

    void LoadConfiguration(Core::System& system);
    void ApplyConfiguration(Core::System& system);

private:
    std::unique_ptr<Ui::ConfigurationHacks> ui;
};
