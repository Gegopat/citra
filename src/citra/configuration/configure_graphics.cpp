// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef __APPLE__
#include <QMessageBox>
#endif
#include <QColorDialog>
#include "citra/configuration/configure_graphics.h"
#include "citra/ui_settings.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_graphics.h"
#include "video_core/renderer/renderer.h"
#include "video_core/video_core.h"

ConfigureGraphics::ConfigureGraphics(QWidget* parent)
    : QWidget{parent}, ui{std::make_unique<Ui::ConfigureGraphics>()} {
    ui->setupUi(this);
    ui->resolution_factor_combobox->addItems(UISettings::resolutions);
}

ConfigureGraphics::~ConfigureGraphics() {}

void ConfigureGraphics::LoadConfiguration(Core::System& system) {
    ui->enable_shadows->setEnabled(!system.IsPoweredOn());
    ui->enable_shadows->setChecked(Settings::values.enable_shadows);
    ui->toggle_frame_limit->setChecked(Settings::values.use_frame_limit);
    ui->frame_limit->setEnabled(Settings::values.use_frame_limit);
    ui->frame_limit->setValue(Settings::values.frame_limit);
    ui->screen_refresh_rate->setValue(Settings::values.screen_refresh_rate);
    ui->min_vertices_per_thread->setValue(Settings::values.min_vertices_per_thread);
    ui->resolution_factor_combobox->setCurrentIndex(Settings::values.resolution_factor - 1);
    ui->toggle_hw_shaders->setChecked(Settings::values.use_hw_shaders);
    ui->toggle_accurate_gs->setChecked(Settings::values.shaders_accurate_gs);
    ui->toggle_accurate_mul->setChecked(Settings::values.shaders_accurate_mul);
    ui->layout_combobox->setCurrentIndex(static_cast<int>(Settings::values.layout_option));
    ui->swap_screens->setChecked(Settings::values.swap_screens);
    bg_color.setRgbF(Settings::values.bg_red, Settings::values.bg_green, Settings::values.bg_blue);
    ui->layout_bg->setStyleSheet(
        QString("QPushButton { background-color: %1 }").arg(bg_color.name()));
    ui->custom_layout->setChecked(Settings::values.custom_layout);
    ui->layout_combobox->setEnabled(!Settings::values.custom_layout);
    ui->custom_layout_group->setVisible(Settings::values.custom_layout);
    ui->custom_top_left->setValue(Settings::values.custom_top_left);
    ui->custom_top_top->setValue(Settings::values.custom_top_top);
    ui->custom_top_right->setValue(Settings::values.custom_top_right);
    ui->custom_top_bottom->setValue(Settings::values.custom_top_bottom);
    ui->custom_bottom_left->setValue(Settings::values.custom_bottom_left);
    ui->custom_bottom_top->setValue(Settings::values.custom_bottom_top);
    ui->custom_bottom_right->setValue(Settings::values.custom_bottom_right);
    ui->custom_bottom_bottom->setValue(Settings::values.custom_bottom_bottom);
    connect(ui->toggle_frame_limit, &QCheckBox::stateChanged, ui->frame_limit,
            &QSpinBox::setEnabled);
    connect(ui->layout_bg, &QPushButton::clicked, this, [this] {
        auto new_color{QColorDialog::getColor(bg_color, this)};
        if (new_color.isValid()) {
            bg_color = new_color;
            ui->layout_bg->setStyleSheet(
                QString("QPushButton { background-color: %1 }").arg(bg_color.name()));
        }
    });
    ui->hw_shaders_group->setEnabled(ui->toggle_hw_shaders->isChecked());
    connect(ui->toggle_hw_shaders, &QCheckBox::stateChanged, ui->hw_shaders_group,
            &QWidget::setEnabled);
    connect(ui->custom_layout, &QCheckBox::stateChanged, ui->custom_layout_group,
            &QWidget::setVisible);
    connect(ui->custom_layout, &QCheckBox::stateChanged, ui->layout_combobox,
            &QWidget::setDisabled);
#ifdef __APPLE__
    connect(ui->toggle_hw_shaders, &QCheckBox::stateChanged, this, [this](int state) {
        if (state == Qt::Checked) {
            QMessageBox::warning(
                this, "Hardware Shaders Warning",
                "Hardware Shaders support is broken on macOS, and will cause graphical issues "
                "like showing a black screen.<br><br>The option is only there for "
                "test/development purposes. If you experience graphical issues with Hardware "
                "Shader, please turn it off.");
        }
    });
#endif
}

void ConfigureGraphics::ApplyConfiguration() {
    Settings::values.enable_shadows = ui->enable_shadows->isChecked();
    Settings::values.use_frame_limit = ui->toggle_frame_limit->isChecked();
    Settings::values.frame_limit = ui->frame_limit->value();
    Settings::values.screen_refresh_rate = ui->screen_refresh_rate->value();
    Settings::values.min_vertices_per_thread = ui->min_vertices_per_thread->value();
    Settings::values.resolution_factor =
        static_cast<u16>(ui->resolution_factor_combobox->currentIndex() + 1);
    Settings::values.use_hw_shaders = ui->toggle_hw_shaders->isChecked();
    Settings::values.shaders_accurate_gs = ui->toggle_accurate_gs->isChecked();
    Settings::values.shaders_accurate_mul = ui->toggle_accurate_mul->isChecked();
    Settings::values.bg_red = bg_color.redF();
    Settings::values.bg_green = bg_color.greenF();
    Settings::values.bg_blue = bg_color.blueF();
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(ui->layout_combobox->currentIndex());
    Settings::values.swap_screens = ui->swap_screens->isChecked();
    Settings::values.custom_layout = ui->custom_layout->isChecked();
    Settings::values.custom_top_left = ui->custom_top_left->value();
    Settings::values.custom_top_top = ui->custom_top_top->value();
    Settings::values.custom_top_right = ui->custom_top_right->value();
    Settings::values.custom_top_bottom = ui->custom_top_bottom->value();
    Settings::values.custom_bottom_left = ui->custom_bottom_left->value();
    Settings::values.custom_bottom_top = ui->custom_bottom_top->value();
    Settings::values.custom_bottom_right = ui->custom_bottom_right->value();
    Settings::values.custom_bottom_bottom = ui->custom_bottom_bottom->value();
}
