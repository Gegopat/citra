// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigurationUi;
} // namespace Ui

class ConfigurationUi : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurationUi(QWidget* parent = nullptr);
    ~ConfigurationUi();

    void ApplyConfiguration();

private:
    void LoadConfiguration();

    std::unique_ptr<Ui::ConfigurationUi> ui;
};
