// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QHash>
#include <QListWidgetItem>
#include "citra/configuration/config.h"
#include "citra/configuration/dialog.h"
#include "citra/hotkeys.h"
#include "core/settings.h"
#include "ui_dialog.h"

ConfigurationDialog::ConfigurationDialog(QWidget* parent, const HotkeyRegistry& registry,
                                         Core::System& system)
    : QDialog{parent}, ui{std::make_unique<Ui::ConfigurationDialog>()}, system{system} {
    ui->setupUi(this);
    ui->generalTab->PopulateHotkeyList(registry);
    PopulateSelectionList();
    connect(ui->generalTab, &ConfigurationGeneral::RestoreDefaultsRequested, [this] {
        restore_defaults_requested = true;
        accept();
    });
    connect(ui->selectorList, &QListWidget::itemSelectionChanged, this,
            &ConfigurationDialog::UpdateVisibleTabs);
    adjustSize();
    ui->selectorList->setCurrentRow(0);
    ui->generalTab->LoadConfiguration(system);
    ui->graphicsTab->LoadConfiguration(system);
    ui->systemTab->LoadConfiguration(system);
    ui->hacksTab->LoadConfiguration(system);
    ui->lleTab->LoadConfiguration(system);
}

ConfigurationDialog::~ConfigurationDialog() {}

void ConfigurationDialog::ApplyConfiguration() {
    ui->generalTab->ApplyConfiguration();
    ui->systemTab->ApplyConfiguration();
    ui->inputTab->ApplyConfiguration();
    ui->inputTab->ApplyProfile();
    ui->graphicsTab->ApplyConfiguration();
    ui->audioTab->ApplyConfiguration();
    ui->cameraTab->ApplyConfiguration();
    ui->hacksTab->ApplyConfiguration(system);
    ui->lleTab->ApplyConfiguration();
    ui->uiTab->ApplyConfiguration();
    Settings::Apply(system);
    Settings::LogSettings();
}

void ConfigurationDialog::PopulateSelectionList() {
    const std::array<std::pair<QString, QStringList>, 4> items{{
        {"General", {"General", "UI"}},
        {"System", {"System", "Audio", "Camera", "Hacks", "LLE"}},
        {"Graphics", {"Graphics"}},
        {"Controls", {"Input"}},
    }};
    for (const auto& entry : items) {
        auto item{new QListWidgetItem(entry.first)};
        item->setData(Qt::UserRole, entry.second);
        ui->selectorList->addItem(item);
    }
}

void ConfigurationDialog::UpdateVisibleTabs() {
    auto items{ui->selectorList->selectedItems()};
    if (items.isEmpty())
        return;
    const QHash<QString, QWidget*> widgets{{
        {"General", ui->generalTab},
        {"System", ui->systemTab},
        {"Input", ui->inputTab},
        {"Graphics", ui->graphicsTab},
        {"Audio", ui->audioTab},
        {"Camera", ui->cameraTab},
        {"Hacks", ui->hacksTab},
        {"LLE", ui->lleTab},
        {"UI", ui->uiTab},
    }};
    ui->tabWidget->clear();
    auto tabs{items[0]->data(Qt::UserRole).toStringList()};
    for (const auto& tab : tabs)
        ui->tabWidget->addTab(widgets[tab], tab);
}
