// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigureAudio;
} // namespace Ui

class ConfigureAudio : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureAudio(QWidget* parent = nullptr);
    ~ConfigureAudio();

    void ApplyConfiguration();

private:
    void LoadConfiguration();

    std::unique_ptr<Ui::ConfigureAudio> ui;
};