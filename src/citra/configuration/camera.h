// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/camera/factory.h"
#include "core/camera/interface.h"

namespace Ui {
class ConfigurationCamera;
} // namespace Ui

class ConfigurationCamera : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurationCamera(QWidget* parent = nullptr);
    ~ConfigurationCamera() override;

    void ApplyConfiguration();
    void timerEvent(QTimerEvent*) override;

private:
    enum class CameraPosition { RearRight, Front, RearLeft, RearBoth, Null };
    static const std::array<std::string, 3> Implementations;

    void LoadConfiguration();
    void OnToolButtonClicked();
    void RecordConfig();        ///< Record the current configuration
    void UpdateCameraMode();    ///< Updates camera mode
    void UpdateImageSourceUI(); ///< Updates image source
    void startPreviewing();
    void stopPreviewing();
    void ConnectEvents();
    CameraPosition GetCameraSelection();
    int GetSelectedCameraIndex();

private:
    std::unique_ptr<Ui::ConfigurationCamera> ui;
    std::array<std::string, 3> camera_name;
    std::array<std::string, 3> camera_config;
    std::array<int, 3> camera_flip;
    int timer_id{};
    int preview_width{};
    int preview_height{};
    CameraPosition current_selected{CameraPosition::Front};
    bool is_previewing{};
    std::unique_ptr<Camera::CameraInterface> previewing_camera;
};
