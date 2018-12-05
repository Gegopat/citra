// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "audio_core/sink.h"
#include "citra/configuration/audio.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_audio.h"

ConfigurationAudio::ConfigurationAudio(QWidget* parent)
    : QWidget{parent}, ui{std::make_unique<Ui::ConfigurationAudio>()} {
    ui->setupUi(this);
    LoadConfiguration();
}

ConfigurationAudio::~ConfigurationAudio() {}

void ConfigurationAudio::LoadConfiguration() {
    ui->toggle_audio_stretching->setChecked(Settings::values.enable_audio_stretching);
    // Load output devices
    ui->output_device_combo_box->addItem("auto");
    std::vector<std::string> device_list{AudioCore::ListDevices()};
    for (const auto& device : device_list)
        ui->output_device_combo_box->addItem(device.c_str());
    int new_device_index{-1};
    for (int index{}; index < ui->output_device_combo_box->count(); index++)
        if (ui->output_device_combo_box->itemText(index).toStdString() ==
            Settings::values.output_device) {
            new_device_index = index;
            break;
        }
    ui->output_device_combo_box->setCurrentIndex(new_device_index);
}

void ConfigurationAudio::ApplyConfiguration() {
    Settings::values.enable_audio_stretching = ui->toggle_audio_stretching->isChecked();
    Settings::values.output_device = ui->output_device_combo_box->currentText().toStdString();
}
