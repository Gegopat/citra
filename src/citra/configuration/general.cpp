// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QMessageBox>
#include "citra/configuration/general.h"
#include "citra/ui_settings.h"
#include "citra/util/util.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_general.h"

ConfigurationGeneral::ConfigurationGeneral(QWidget* parent)
    : QWidget{parent}, ui{std::make_unique<Ui::ConfigurationGeneral>()} {
    ui->setupUi(this);
#ifndef _WIN32
    ui->toggle_console->setText("Enable logging to console");
    ui->toggle_console->setToolTip(QString());
#endif
    connect(ui->restore_defaults, &QPushButton::released, this, [this] {
        auto answer{QMessageBox::question(
            this, "Citra",
            "Are you sure you want to <b>restore your settings to default</b>?<br>Hotkeys will not "
            "be reset until you restart Citra.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)};
        if (answer == QMessageBox::No)
            return;
        emit RestoreDefaultsRequested();
    });
}

ConfigurationGeneral::~ConfigurationGeneral() {}

void ConfigurationGeneral::LoadConfiguration(Core::System& system) {
    ui->toggle_console->setEnabled(!system.IsPoweredOn());
    ui->combobox_keyboard_mode->setCurrentIndex(static_cast<int>(Settings::values.keyboard_mode));
    ui->toggle_console->setChecked(UISettings::values.show_console);
    ui->log_filter_edit->setText(QString::fromStdString(Settings::values.log_filter));
    ui->confirm_close->setChecked(UISettings::values.confirm_close);
}

void ConfigurationGeneral::ApplyConfiguration() {
    Settings::values.keyboard_mode =
        static_cast<Settings::KeyboardMode>(ui->combobox_keyboard_mode->currentIndex());
    UISettings::values.show_console = ui->toggle_console->isChecked();
    Settings::values.log_filter = ui->log_filter_edit->text().toStdString();
    UISettings::values.confirm_close = ui->confirm_close->isChecked();
    ToggleConsole();
    Log::Filter filter;
    filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(filter);
}
