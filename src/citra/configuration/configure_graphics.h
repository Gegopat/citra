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
class ConfigureGraphics;
} // namespace Ui

class ConfigureGraphics : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureGraphics(QWidget* parent = nullptr);
    ~ConfigureGraphics();

    void LoadConfiguration(Core::System& system);
    void ApplyConfiguration();

private:
    QColor bg_color;
    std::unique_ptr<Ui::ConfigureGraphics> ui;
};
