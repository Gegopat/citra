// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>
#include "common/param_package.h"
#include "input_common/udp/udp.h"

namespace Ui {
class ConfigurationMotionTouch;
} // namespace Ui

class QVBoxLayout;
class QLabel;
class QPushButton;

/// A dialog for touchpad calibration configuration.
class CalibrationConfigurationDialog : public QDialog {
    Q_OBJECT

public:
    explicit CalibrationConfigurationDialog(QWidget* parent, const std::string& host, u16 port,
                                            u8 pad_index, u16 client_id);
    ~CalibrationConfigurationDialog();

private:
    Q_INVOKABLE void UpdateLabelText(QString text);
    Q_INVOKABLE void UpdateButtonText(QString text);

    QVBoxLayout* layout;
    QLabel* status_label;
    QPushButton* cancel_button;
    std::unique_ptr<InputCommon::CemuhookUDP::CalibrationConfigurationJob> job;

    // Configuration results
    bool completed{};
    u16 min_x, min_y, max_x, max_y;

    friend class ConfigurationMotionTouch;
};

class ConfigurationMotionTouch : public QDialog {
    Q_OBJECT

public:
    explicit ConfigurationMotionTouch(QWidget* parent = nullptr);
    ~ConfigurationMotionTouch();

public slots:
    void ApplyConfiguration();

private slots:
    void OnCemuhookUDPTest();
    void OnConfigurationTouchCalibration();

private:
    void closeEvent(QCloseEvent* event) override;

    Q_INVOKABLE void ShowUDPTestResult(bool result);

    void LoadConfiguration();
    void UpdateUIDisplay();
    void ConnectEvents();
    bool CanCloseDialog();

    std::unique_ptr<Ui::ConfigurationMotionTouch> ui;
    int min_x, min_y, max_x, max_y; ///< Coordinate system of the CemuhookUDP touch provider
    bool udp_test_in_progress{};
};
