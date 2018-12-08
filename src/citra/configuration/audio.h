// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigurationAudio;
} // namespace Ui

class ConfigurationAudio : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurationAudio(QWidget* parent = nullptr);
    ~ConfigurationAudio() override;

    void ApplyConfiguration();

private:
    void LoadConfiguration();

    std::unique_ptr<Ui::ConfigurationAudio> ui;
};
