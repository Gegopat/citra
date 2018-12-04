// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra/configuration/configure_ui.h"
#include "citra/ui_settings.h"
#include "ui_configure_ui.h"

ConfigureUi::ConfigureUi(QWidget* parent)
    : QWidget(parent), ui{std::make_unique<Ui::ConfigureUi>()} {
    ui->setupUi(this);
#ifndef ENABLE_DISCORD_RPC
    ui->enable_discord_rpc->hide();
#endif
    for (const auto& theme : UISettings::themes)
        ui->theme_combobox->addItem(theme.first, theme.second);
    LoadConfiguration();
}

ConfigureUi::~ConfigureUi() = default;

void ConfigureUi::LoadConfiguration() {
    ui->enable_discord_rpc->setChecked(UISettings::values.enable_discord_rpc);
    ui->theme_combobox->setCurrentIndex(ui->theme_combobox->findData(UISettings::values.theme));
    ui->icon_size_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.program_list_icon_size));
    ui->row_1_text_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.program_list_row_1));
    ui->row_2_text_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.program_list_row_2) + 1);
    ui->toggle_hide_no_icon->setChecked(UISettings::values.program_list_hide_no_icon);
}

void ConfigureUi::ApplyConfiguration() {
    UISettings::values.enable_discord_rpc = ui->enable_discord_rpc->isChecked();
    UISettings::values.theme =
        ui->theme_combobox->itemData(ui->theme_combobox->currentIndex()).toString();
    UISettings::values.program_list_icon_size =
        static_cast<UISettings::ProgramListIconSize>(ui->icon_size_combobox->currentIndex());
    UISettings::values.program_list_row_1 =
        static_cast<UISettings::ProgramListText>(ui->row_1_text_combobox->currentIndex());
    UISettings::values.program_list_row_2 =
        static_cast<UISettings::ProgramListText>(ui->row_2_text_combobox->currentIndex() - 1);
    UISettings::values.program_list_hide_no_icon = ui->toggle_hide_no_icon->isChecked();
}
