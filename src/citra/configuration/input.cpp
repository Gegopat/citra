// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <utility>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include "citra/configuration/config.h"
#include "citra/configuration/input.h"
#include "citra/configuration/motion_touch.h"
#include "common/param_package.h"

const std::array<std::string, ConfigurationInput::ANALOG_SUB_BUTTONS_NUM>
    ConfigurationInput::analog_sub_buttons{{
        "up",
        "down",
        "left",
        "right",
        "modifier",
    }};

static QString GetKeyName(int key_code) {
    switch (key_code) {
    case Qt::Key_Shift:
        return "Shift";
    case Qt::Key_Control:
        return "Ctrl";
    case Qt::Key_Alt:
        return "Alt";
    case Qt::Key_Meta:
        return "";
    default:
        return QKeySequence{key_code}.toString();
    }
}

static void SetAnalogButton(const Common::ParamPackage& input_param,
                            Common::ParamPackage& analog_param, const std::string& button_name) {
    if (analog_param.Get("engine", "") != "analog_from_button")
        analog_param = {
            {"engine", "analog_from_button"},
            {"modifier_scale", "0.5"},
        };
    analog_param.Set(button_name, input_param.Serialize());
}

static QString ButtonToText(const Common::ParamPackage& param) {
    if (!param.Has("engine"))
        return "[not set]";
    else if (param.Get("engine", "") == "keyboard")
        return GetKeyName(param.Get("code", 0));
    else if (param.Get("engine", "") == "sdl") {
        if (param.Has("hat"))
            return QString("Hat %1 %2")
                .arg(QString::fromStdString(param.Get("hat", "")),
                     QString::fromStdString(param.Get("direction", "")));
        if (param.Has("axis"))
            return QString("Axis %1%2")
                .arg(QString::fromStdString(param.Get("axis", "")),
                     QString::fromStdString(param.Get("direction", "")));
        if (param.Has("button"))
            return QString("Button %1").arg(QString::fromStdString(param.Get("button", "")));
        return QString();
    } else
        return "[unknown]";
};

static QString AnalogToText(const Common::ParamPackage& param, const std::string& dir) {
    if (!param.Has("engine"))
        return "[not set]";
    else if (param.Get("engine", "") == "analog_from_button")
        return ButtonToText(Common::ParamPackage{param.Get(dir, "")});
    else if (param.Get("engine", "") == "sdl") {
        if (dir == "modifier")
            return "[unused]";

        if (dir == "left" || dir == "right")
            return QString("Axis %1").arg(QString::fromStdString(param.Get("axis_x", "")));
        else if (dir == "up" || dir == "down")
            return QString("Axis %1").arg(QString::fromStdString(param.Get("axis_y", "")));
        return QString();
    } else {
        return "[unknown]";
    }
};

ConfigurationInput::ConfigurationInput(QWidget* parent)
    : QWidget{parent}, ui{std::make_unique<Ui::ConfigurationInput>()},
      timeout_timer{std::make_unique<QTimer>()}, poll_timer{std::make_unique<QTimer>()} {
    ui->setupUi(this);
    setFocusPolicy(Qt::ClickFocus);
    for (int i{}; i < Settings::values.profiles.size(); ++i)
        ui->profile->addItem(QString::fromStdString(Settings::values.profiles[i].name));
    ui->profile->setCurrentIndex(Settings::values.profile);
    button_map = {
        ui->buttonA,        ui->buttonB,        ui->buttonX,         ui->buttonY,  ui->buttonDpadUp,
        ui->buttonDpadDown, ui->buttonDpadLeft, ui->buttonDpadRight, ui->buttonL,  ui->buttonR,
        ui->buttonStart,    ui->buttonSelect,   ui->buttonZL,        ui->buttonZR, ui->buttonHome,
    };
    analog_map_buttons = {{
        {
            ui->buttonCircleUp,
            ui->buttonCircleDown,
            ui->buttonCircleLeft,
            ui->buttonCircleRight,
            ui->buttonCircleMod,
        },
        {
            ui->buttonCStickUp,
            ui->buttonCStickDown,
            ui->buttonCStickLeft,
            ui->buttonCStickRight,
            nullptr,
        },
    }};
    analog_map_stick = {ui->buttonCircleAnalog, ui->buttonCStickAnalog};
    for (int button_id{}; button_id < Settings::NativeButton::NumButtons; button_id++) {
        if (!button_map[button_id])
            continue;
        button_map[button_id]->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button_map[button_id], &QPushButton::released, [=]() {
            HandleClick(button_map[button_id],
                        [=](const Common::ParamPackage& params) {
                            buttons_param[button_id] = params;
                            ApplyConfiguration();
                            Settings::SaveProfile(ui->profile->currentIndex());
                        },
                        InputCommon::Polling::DeviceType::Button);
        });
        connect(button_map[button_id], &QPushButton::customContextMenuRequested,
                [=](const QPoint& menu_location) {
                    QMenu context_menu;
                    QAction* clear_action{context_menu.addAction("Clear")};
                    QAction* restore_default_action{context_menu.addAction("Restore Default")};

                    connect(clear_action, &QAction::triggered, [&] {
                        buttons_param[button_id].Clear();
                        button_map[button_id]->setText("[not set]");
                        ApplyConfiguration();
                        Settings::SaveProfile(ui->profile->currentIndex());
                    });
                    connect(restore_default_action, &QAction::triggered, [&] {
                        buttons_param[button_id] = Common::ParamPackage{
                            InputCommon::GenerateKeyboardParam(Config::default_buttons[button_id])};
                        button_map[button_id]->setText(ButtonToText(buttons_param[button_id]));
                        ApplyConfiguration();
                        Settings::SaveProfile(ui->profile->currentIndex());
                    });
                    context_menu.exec(button_map[button_id]->mapToGlobal(menu_location));
                });
    }

    for (int analog_id{}; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id{}; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            if (!analog_map_buttons[analog_id][sub_button_id])
                continue;
            analog_map_buttons[analog_id][sub_button_id]->setContextMenuPolicy(
                Qt::CustomContextMenu);
            connect(analog_map_buttons[analog_id][sub_button_id], &QPushButton::released, [=]() {
                HandleClick(analog_map_buttons[analog_id][sub_button_id],
                            [=](const Common::ParamPackage& params) {
                                SetAnalogButton(params, analogs_param[analog_id],
                                                analog_sub_buttons[sub_button_id]);
                                ApplyConfiguration();
                                Settings::SaveProfile(ui->profile->currentIndex());
                            },
                            InputCommon::Polling::DeviceType::Button);
            });
            connect(analog_map_buttons[analog_id][sub_button_id],
                    &QPushButton::customContextMenuRequested, [=](const QPoint& menu_location) {
                        QMenu context_menu;
                        QAction* clear_action{context_menu.addAction("Clear")};
                        QAction* restore_default_action{context_menu.addAction("Restore Default")};
                        connect(clear_action, &QAction::triggered, [=] {
                            analogs_param[analog_id].Erase(analog_sub_buttons[sub_button_id]);
                            analog_map_buttons[analog_id][sub_button_id]->setText("[not set]");
                            ApplyConfiguration();
                            Settings::SaveProfile(ui->profile->currentIndex());
                        });
                        connect(restore_default_action, &QAction::triggered, [=] {
                            Common::ParamPackage params{InputCommon::GenerateKeyboardParam(
                                Config::default_analogs[analog_id][sub_button_id])};
                            SetAnalogButton(params, analogs_param[analog_id],
                                            analog_sub_buttons[sub_button_id]);
                            analog_map_buttons[analog_id][sub_button_id]->setText(AnalogToText(
                                analogs_param[analog_id], analog_sub_buttons[sub_button_id]));
                            ApplyConfiguration();
                            Settings::SaveProfile(ui->profile->currentIndex());
                        });
                        context_menu.exec(analog_map_buttons[analog_id][sub_button_id]->mapToGlobal(
                            menu_location));
                    });
        }
        connect(analog_map_stick[analog_id], &QPushButton::released, [=]() {
            QMessageBox::information(
                this, "Information",
                "After pressing OK, first move your joystick horizontally, and then vertically.");
            HandleClick(analog_map_stick[analog_id],
                        [=](const Common::ParamPackage& params) {
                            analogs_param[analog_id] = params;
                            ApplyConfiguration();
                            Settings::SaveProfile(ui->profile->currentIndex());
                        },
                        InputCommon::Polling::DeviceType::Analog);
        });
    }
    connect(ui->buttonMotionTouch, &QPushButton::released, [this] {
        auto motion_touch_dialog{new ConfigurationMotionTouch(this)};
        return motion_touch_dialog->exec();
    });
    connect(ui->buttonClearAll, &QPushButton::released, [this] { ClearAll(); });
    connect(ui->buttonRestoreDefaults, &QPushButton::released, [this] { RestoreDefaults(); });
    connect(ui->buttonNew, &QPushButton::released, [this] { OnNewProfile(); });
    connect(ui->buttonDelete, &QPushButton::released, [this] { OnDeleteProfile(); });
    connect(ui->buttonRename, &QPushButton::released, [this] { OnRenameProfile(); });
    connect(ui->profile, qOverload<int>(&QComboBox::currentIndexChanged), [this](int i) {
        ApplyConfiguration();
        Settings::SaveProfile(Settings::values.profile);
        Settings::LoadProfile(i);
        LoadConfiguration();
    });
    timeout_timer->setSingleShot(true);
    connect(timeout_timer.get(), &QTimer::timeout, [this]() { SetPollingResult({}, true); });
    connect(poll_timer.get(), &QTimer::timeout, [this]() {
        Common::ParamPackage params{};
        for (auto& poller : device_pollers) {
            params = poller->GetNextInput();
            if (params.Has("engine")) {
                SetPollingResult(params, false);
                return;
            }
        }
    });
    LoadConfiguration();
}

ConfigurationInput::~ConfigurationInput() = default;

void ConfigurationInput::EmitInputKeysChanged() {
    emit InputKeysChanged(GetUsedKeyboardKeys());
}

void ConfigurationInput::OnHotkeysChanged(QList<QKeySequence> new_key_list) {
    hotkey_list = new_key_list;
}

QList<QKeySequence> ConfigurationInput::GetUsedKeyboardKeys() {
    QList<QKeySequence> list;
    for (int button{}; button < Settings::NativeButton::NumButtons; button++) {
        auto button_param{buttons_param[button]};
        if (button_param.Get("engine", "") == "keyboard")
            list << QKeySequence{button_param.Get("code", 0)};
    }
    return list;
}

void ConfigurationInput::LoadConfiguration() {
    std::transform(Settings::values.buttons.begin(), Settings::values.buttons.end(),
                   buttons_param.begin(),
                   [](const std::string& str) { return Common::ParamPackage(str); });
    std::transform(Settings::values.analogs.begin(), Settings::values.analogs.end(),
                   analogs_param.begin(),
                   [](const std::string& str) { return Common::ParamPackage(str); });
    UpdateButtonLabels();
}

void ConfigurationInput::ApplyConfiguration() {
    std::transform(buttons_param.begin(), buttons_param.end(), Settings::values.buttons.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });
    std::transform(analogs_param.begin(), analogs_param.end(), Settings::values.analogs.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });
}

void ConfigurationInput::ApplyProfile() {
    Settings::values.profile = ui->profile->currentIndex();
}

void ConfigurationInput::RestoreDefaults() {
    for (int button_id{}; button_id < Settings::NativeButton::NumButtons; button_id++) {
        buttons_param[button_id] = Common::ParamPackage{
            InputCommon::GenerateKeyboardParam(Config::default_buttons[button_id])};
    }

    for (int analog_id{}; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id{}; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            Common::ParamPackage params{InputCommon::GenerateKeyboardParam(
                Config::default_analogs[analog_id][sub_button_id])};
            SetAnalogButton(params, analogs_param[analog_id], analog_sub_buttons[sub_button_id]);
        }
    }
    UpdateButtonLabels();
}

void ConfigurationInput::ClearAll() {
    for (int button_id{}; button_id < Settings::NativeButton::NumButtons; button_id++)
        if (button_map[button_id] && button_map[button_id]->isEnabled())
            buttons_param[button_id].Clear();
    for (int analog_id{}; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++)
        for (int sub_button_id{}; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++)
            if (analog_map_buttons[analog_id][sub_button_id] &&
                analog_map_buttons[analog_id][sub_button_id]->isEnabled())
                analogs_param[analog_id].Erase(analog_sub_buttons[sub_button_id]);
    UpdateButtonLabels();
}

void ConfigurationInput::UpdateButtonLabels() {
    for (int button{}; button < Settings::NativeButton::NumButtons; button++)
        button_map[button]->setText(ButtonToText(buttons_param[button]));
    for (int analog_id{}; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id{}; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++)
            if (analog_map_buttons[analog_id][sub_button_id])
                analog_map_buttons[analog_id][sub_button_id]->setText(
                    AnalogToText(analogs_param[analog_id], analog_sub_buttons[sub_button_id]));
        analog_map_stick[analog_id]->setText("Set Analog Stick");
    }
    EmitInputKeysChanged();
}

void ConfigurationInput::HandleClick(
    QPushButton* button, std::function<void(const Common::ParamPackage&)> new_input_setter,
    InputCommon::Polling::DeviceType type) {
    previous_key_code = QKeySequence{button->text()}[0];
    button->setText("[press key]");
    button->setFocus();
    input_setter = new_input_setter;
    device_pollers = InputCommon::Polling::GetPollers(type);
    // Keyboard keys can only be used as button devices
    want_keyboard_keys = type == InputCommon::Polling::DeviceType::Button;
    for (auto& poller : device_pollers)
        poller->Start();
    grabKeyboard();
    grabMouse();
    timeout_timer->start(5000); // Cancel after 5 seconds
    poll_timer->start(200);     // Check for new inputs every 200ms
}

void ConfigurationInput::SetPollingResult(const Common::ParamPackage& params, bool abort) {
    releaseKeyboard();
    releaseMouse();
    timeout_timer->stop();
    poll_timer->stop();
    for (auto& poller : device_pollers)
        poller->Stop();
    if (!abort && input_setter)
        (*input_setter)(params);
    UpdateButtonLabels();
    input_setter.reset();
}

void ConfigurationInput::keyPressEvent(QKeyEvent* event) {
    if (!input_setter || !event)
        return;
    if (event->key() != Qt::Key_Escape && event->key() != previous_key_code) {
        if (want_keyboard_keys) {
            // Check if key is already bound
            if (hotkey_list.contains(QKeySequence{event->key()}) ||
                GetUsedKeyboardKeys().contains(QKeySequence{event->key()})) {
                SetPollingResult({}, true);
                QMessageBox::critical(this, "Error!", "You're using a key that's already bound.");
                return;
            }
            SetPollingResult(Common::ParamPackage{InputCommon::GenerateKeyboardParam(event->key())},
                             false);
        } else
            // Escape key wasn't pressed and we don't want any keyboard keys, so don't stop
            // polling
            return;
    }
    SetPollingResult({}, true);
    previous_key_code = 0;
}

void ConfigurationInput::OnNewProfile() {
    auto name{QInputDialog::getText(this, "New Profile", "Enter the name for the new profile.")};
    if (name.isEmpty())
        return;
    ApplyConfiguration();
    Settings::SaveProfile(ui->profile->currentIndex());
    Settings::CreateProfile(name.toStdString());
    ui->profile->addItem(name);
    ui->profile->setCurrentIndex(Settings::values.profile);
    LoadConfiguration();
}

void ConfigurationInput::OnDeleteProfile() {
    if (ui->profile->count() == 1) {
        QMessageBox::critical(this, "Citra", "You need to have 1 profile at least");
        return;
    }
    auto answer{QMessageBox::question(
        this, "Delete Profile", QString("Delete profile %1?").arg(ui->profile->currentText()))};
    if (answer != QMessageBox::Yes)
        return;
    int index{ui->profile->currentIndex()};
    ui->profile->removeItem(index);
    ui->profile->setCurrentIndex(0);
    Settings::DeleteProfile(index);
    LoadConfiguration();
}

void ConfigurationInput::OnRenameProfile() {
    auto new_name{QInputDialog::getText(this, "Rename Profile", "New name:")};
    if (new_name.isEmpty())
        return;
    ui->profile->setItemText(ui->profile->currentIndex(), new_name);
    Settings::RenameCurrentProfile(new_name.toStdString());
}
