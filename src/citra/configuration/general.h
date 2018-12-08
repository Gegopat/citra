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
class ConfigurationGeneral;
} // namespace Ui

class ConfigurationGeneral : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurationGeneral(QWidget* parent = nullptr);
    ~ConfigurationGeneral() override;

    void LoadConfiguration(Core::System& system);
    void ApplyConfiguration();

signals:
    void RestoreDefaultsRequested();

private:
    std::unique_ptr<Ui::ConfigurationGeneral> ui;
};
