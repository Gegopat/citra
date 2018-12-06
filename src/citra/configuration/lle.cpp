// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra/configuration/lle.h"
#include "core/core.h"
#include "core/hle/service/service.h"
#include "core/settings.h"
#include "ui_lle.h"

ConfigurationLLE::ConfigurationLLE(QWidget* parent)
    : QWidget{parent}, ui{std::make_unique<Ui::ConfigurationLLE>()} {
    ui->setupUi(this);
}

ConfigurationLLE::~ConfigurationLLE() {}

void ConfigurationLLE::LoadConfiguration(Core::System& system) {
    bool allow_changes{!system.IsPoweredOn()};
    ui->use_lle_applets->setEnabled(allow_changes);
    ui->use_lle_dsp->setEnabled(allow_changes);
    ui->use_lle_applets->setChecked(Settings::values.use_lle_applets);
    ui->use_lle_dsp->setChecked(Settings::values.use_lle_dsp);
    for (const auto& module : Service::service_module_map) {
        auto checkbox{new QCheckBox(QString::fromStdString(module.name))};
        checkbox->setEnabled(allow_changes);
        checkbox->setChecked(Settings::values.lle_modules.at(module.name));
        ui->lle_modules->addWidget(checkbox);
        module_checkboxes.append(checkbox);
    }
}

void ConfigurationLLE::ApplyConfiguration() {
    Settings::values.use_lle_applets = ui->use_lle_applets->isChecked();
    Settings::values.use_lle_dsp = ui->use_lle_dsp->isChecked();
    for (const auto& checkbox : module_checkboxes)
        Settings::values.lle_modules.at(checkbox->text().toStdString()) = checkbox->isChecked();
}
