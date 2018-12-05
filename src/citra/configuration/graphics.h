// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Core {
class System;
} // namespace Core

namespace Ui {
class ConfigurationGraphics;
} // namespace Ui

class ConfigurationGraphics : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurationGraphics(QWidget* parent = nullptr);
    ~ConfigurationGraphics();

    void LoadConfiguration(Core::System& system);
    void ApplyConfiguration();

private:
    QColor bg_color;
    std::unique_ptr<Ui::ConfigurationGraphics> ui;
};
