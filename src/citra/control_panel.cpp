// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QComboBox>
#include "citra/control_panel.h"
#include "core/core.h"
#include "core/hle/kernel/shared_page.h"
#include "core/settings.h"
#include "ui_control_panel.h"

namespace SharedPageUtil {

static int NetworkStateToIndex(SharedPage::NetworkState state) {
    switch (state) {
    case SharedPage::NetworkState::Internet:
        return 3;
    case SharedPage::NetworkState::Local:
        return 2;
    case SharedPage::NetworkState::Disabled:
        return 1;
    case SharedPage::NetworkState::Enabled:
        return 0;
    default:
        UNREACHABLE();
    }
}

static SharedPage::NetworkState IndexToNetworkState(int index) {
    switch (index) {
    case 0:
        return SharedPage::NetworkState::Enabled;
    case 1:
        return SharedPage::NetworkState::Disabled;
    case 2:
        return SharedPage::NetworkState::Local;
    case 3:
        return SharedPage::NetworkState::Internet;
    }
    return SharedPage::NetworkState::Enabled;
}

} // namespace SharedPageUtil

ControlPanel::ControlPanel(Core::System& system, QWidget* parent)
    : QDialog{parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint},
      ui{std::make_unique<Ui::ControlPanel>()} {
    ui->setupUi(this);
    ui->volume_slider->setValue(Settings::values.volume * ui->volume_slider->maximum());
    Update3D();
    ui->slider_3d->setEnabled(!Settings::values.disable_mh_2xmsaa);
    ui->checkbox_headphones_connected->setChecked(Settings::values.headphones_connected);
    ui->power_adapter_connected->setChecked(Settings::values.p_adapter_connected);
    ui->power_battery_charging->setChecked(Settings::values.p_battery_charging);
    ui->power_battery_level->setCurrentIndex(Settings::values.p_battery_level - 1);
    UpdateNetwork();
    connect(ui->power_adapter_connected, &QCheckBox::stateChanged, [&system, this] {
        Settings::values.p_adapter_connected = ui->power_adapter_connected->isChecked();
        if (system.IsPoweredOn())
            system.Kernel().GetSharedPageHandler().SetAdapterConnected(
                static_cast<u8>(Settings::values.p_adapter_connected));
    });
    connect(ui->power_battery_charging, &QCheckBox::stateChanged, [&system, this] {
        Settings::values.p_battery_charging = ui->power_battery_charging->isChecked();
        if (system.IsPoweredOn())
            system.Kernel().GetSharedPageHandler().SetBatteryCharging(
                static_cast<u8>(Settings::values.p_battery_charging));
    });
    connect(ui->power_battery_level, qOverload<int>(&QComboBox::currentIndexChanged),
            [&system, this] {
                Settings::values.p_battery_level =
                    static_cast<u32>(ui->power_battery_level->currentIndex() + 1);
                if (system.IsPoweredOn())
                    system.Kernel().GetSharedPageHandler().SetBatteryLevel(
                        static_cast<u8>(Settings::values.p_battery_level));
            });
    connect(ui->network_wifi_status, qOverload<int>(&QComboBox::currentIndexChanged),
            [this] { Settings::values.n_wifi_status = ui->network_wifi_status->currentIndex(); });
    connect(ui->network_link_level, qOverload<int>(&QComboBox::currentIndexChanged),
            [&system, this] {
                Settings::values.n_wifi_link_level =
                    static_cast<u8>(ui->network_link_level->currentIndex());
                if (system.IsPoweredOn())
                    system.Kernel().GetSharedPageHandler().SetWiFiLinkLevel(
                        static_cast<SharedPage::WiFiLinkLevel>(Settings::values.n_wifi_link_level));
            });
    connect(ui->network_state, qOverload<int>(&QComboBox::currentIndexChanged), [&system, this] {
        Settings::values.n_state =
            static_cast<u8>(SharedPageUtil::IndexToNetworkState(ui->network_state->currentIndex()));
        if (system.IsPoweredOn())
            system.Kernel().GetSharedPageHandler().SetNetworkState(
                static_cast<SharedPage::NetworkState>(Settings::values.n_state));
    });
    connect(ui->volume_slider, &QSlider::valueChanged, [this] {
        Settings::values.volume =
            static_cast<float>(ui->volume_slider->sliderPosition()) / ui->volume_slider->maximum();
    });
    connect(ui->slider_3d, &QSlider::valueChanged, [&system, this] {
        Settings::values.factor_3d = ui->slider_3d->value();
        if (system.IsPoweredOn())
            system.Kernel().GetSharedPageHandler().Update3DSettings(true);
    });
    connect(ui->checkbox_headphones_connected, &QCheckBox::stateChanged, [this] {
        Settings::values.headphones_connected = ui->checkbox_headphones_connected->isChecked();
    });
    setWindowTitle("Control Panel");
}

ControlPanel::~ControlPanel() = default;

void ControlPanel::Update3D() {
    ui->slider_3d->setEnabled(!Settings::values.disable_mh_2xmsaa);
    ui->slider_3d->setValue(Settings::values.factor_3d);
}

void ControlPanel::UpdateNetwork() {
    ui->network_wifi_status->setCurrentIndex(Settings::values.n_wifi_status);
    ui->network_link_level->setCurrentIndex(Settings::values.n_wifi_link_level);
    ui->network_state->setCurrentIndex(SharedPageUtil::NetworkStateToIndex(
        static_cast<SharedPage::NetworkState>(Settings::values.n_state)));
}
