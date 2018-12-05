// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <QCameraInfo>
#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include <QWidget>
#include "citra/configuration/camera.h"
#include "citra/ui_settings.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_camera.h"

const std::array<std::string, 3> ConfigurationCamera::Implementations{{
    "blank", /* Blank */
    "image", /* Image */
    "qt"     /* System Camera */
}};

ConfigurationCamera::ConfigurationCamera(QWidget* parent)
    : QWidget{parent}, ui{std::make_unique<Ui::ConfigurationCamera>()} {
    ui->setupUi(this);
    // Load settings
    camera_name = Settings::values.camera_name;
    camera_config = Settings::values.camera_config;
    camera_flip = Settings::values.camera_flip;
    auto cameras{QCameraInfo::availableCameras()};
    for (const QCameraInfo& cameraInfo : cameras)
        ui->system_camera->addItem(cameraInfo.deviceName());
    UpdateCameraMode();
    LoadConfiguration();
    ConnectEvents();
    ui->preview_box->setHidden(true);
}

ConfigurationCamera::~ConfigurationCamera() {
    stopPreviewing();
}

void ConfigurationCamera::ConnectEvents() {
    connect(ui->image_source, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        stopPreviewing();
        UpdateImageSourceUI();
    });
    connect(ui->camera_selection, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        stopPreviewing();
        if (GetCameraSelection() != current_selected)
            RecordConfig();
        if (ui->camera_selection->currentIndex() == 1) {
            ui->camera_mode->setCurrentIndex(1); // Double
            if (camera_name[0] == camera_name[2] && camera_config[0] == camera_config[2])
                ui->camera_mode->setCurrentIndex(0); // Single
        }
        UpdateCameraMode();
        LoadConfiguration();
    });
    connect(ui->camera_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        stopPreviewing();
        ui->camera_position_label->setVisible(ui->camera_mode->currentIndex() == 1);
        ui->camera_position->setVisible(ui->camera_mode->currentIndex() == 1);
        current_selected = GetCameraSelection();
    });
    connect(ui->camera_position, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        stopPreviewing();
        if (GetCameraSelection() != current_selected)
            RecordConfig();
        LoadConfiguration();
    });
    connect(ui->toolButton, &QToolButton::clicked, this, &ConfigurationCamera::OnToolButtonClicked);
    connect(ui->preview_button, &QPushButton::clicked, this, [&] { startPreviewing(); });
    connect(ui->prompt_before_load, &QCheckBox::stateChanged, this, [this](int state) {
        ui->camera_file->setDisabled(state == Qt::Checked);
        ui->toolButton->setDisabled(state == Qt::Checked);
        if (state == Qt::Checked)
            ui->camera_file->clear();
    });
    connect(ui->camera_file, &QLineEdit::textChanged, this, [&] { stopPreviewing(); });
    connect(ui->system_camera, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [&] { stopPreviewing(); });
    connect(ui->camera_flip, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [&] { stopPreviewing(); });
}

void ConfigurationCamera::UpdateCameraMode() {
    CameraPosition pos{GetCameraSelection()};
    // Set the visibility of the camera mode selection widgets
    if (pos == CameraPosition::RearBoth) {
        ui->camera_position->setHidden(true);
        ui->camera_position_label->setHidden(true);
        ui->camera_mode->setHidden(false);
        ui->camera_mode_label->setHidden(false);
    } else {
        ui->camera_position->setHidden(pos == CameraPosition::Front);
        ui->camera_position_label->setHidden(pos == CameraPosition::Front);
        ui->camera_mode->setHidden(pos == CameraPosition::Front);
        ui->camera_mode_label->setHidden(pos == CameraPosition::Front);
    }
}

void ConfigurationCamera::UpdateImageSourceUI() {
    int image_source{ui->image_source->currentIndex()};
    switch (image_source) {
    case 0: /* Blank */
    case 2: /* System Camera */
        ui->prompt_before_load->setHidden(true);
        ui->prompt_before_load->setChecked(false);
        ui->camera_file_label->setHidden(true);
        ui->camera_file->setHidden(true);
        ui->camera_file->clear();
        ui->toolButton->setHidden(true);
        break;
    case 1: /* Still Image */
        ui->prompt_before_load->setHidden(false);
        ui->camera_file_label->setHidden(false);
        ui->camera_file->setHidden(false);
        ui->toolButton->setHidden(false);
        if (camera_config[GetSelectedCameraIndex()].empty()) {
            ui->prompt_before_load->setChecked(true);
            ui->camera_file->setDisabled(true);
            ui->toolButton->setDisabled(true);
            ui->camera_file->clear();
        } else {
            ui->camera_file->setDisabled(false);
            ui->toolButton->setDisabled(false);
        }
        break;
    default:
        LOG_ERROR(Service_CAM, "Unknown image source {}", image_source);
    }
    ui->system_camera_label->setHidden(image_source != 2);
    ui->system_camera->setHidden(image_source != 2);
    ui->camera_flip_label->setHidden(image_source == 0);
    ui->camera_flip->setHidden(image_source == 0);
}

void ConfigurationCamera::RecordConfig() {
    auto implementation{Implementations[ui->image_source->currentIndex()]};
    int image_source{ui->image_source->currentIndex()};
    std::string config;
    if (image_source == 2) { /* system camera */
        if (ui->system_camera->currentIndex() == 0)
            config = "";
        else
            config = ui->system_camera->currentText().toStdString();
    } else
        config = ui->camera_file->text().toStdString();
    if (current_selected == CameraPosition::RearBoth) {
        camera_name[0] = camera_name[2] = implementation;
        camera_config[0] = camera_config[2] = config;
        camera_flip[0] = camera_flip[2] = ui->camera_flip->currentIndex();
    } else if (current_selected != CameraPosition::Null) {
        int index{static_cast<int>(current_selected)};
        camera_name[index] = implementation;
        camera_config[index] = config;
        camera_flip[index] = ui->camera_flip->currentIndex();
    }
    current_selected = GetCameraSelection();
}

void ConfigurationCamera::startPreviewing() {
    current_selected = GetCameraSelection();
    RecordConfig();
    int camera_selection{GetSelectedCameraIndex()};
    stopPreviewing();
    // Init preview box
    ui->preview_box->setHidden(false);
    ui->preview_button->setHidden(true);
    preview_width = ui->preview_box->size().width();
    preview_height = preview_width * 0.75;
    ui->preview_box->setToolTip(QString("Resolution: %1 * %2").arg(preview_width, preview_height));
    // Load previewing camera
    previewing_camera = Camera::CreateCameraPreview(
        camera_name[camera_selection], camera_config[camera_selection], preview_width,
        preview_height, static_cast<Service::CAM::Flip>(camera_flip[camera_selection]));
    if (!previewing_camera) {
        stopPreviewing();
        return;
    }
    previewing_camera->SetResolution(
        {static_cast<u16>(preview_width), static_cast<u16>(preview_height)});
    previewing_camera->SetEffect(Service::CAM::Effect::None);
    previewing_camera->SetFlip(Service::CAM::Flip::None);
    previewing_camera->SetFormat(Service::CAM::OutputFormat::RGB565);
    previewing_camera->SetFrameRate(Service::CAM::FrameRate::Rate_30);
    previewing_camera->StartCapture();
    timer_id = startTimer(1000 / 30);
}

void ConfigurationCamera::stopPreviewing() {
    ui->preview_box->setHidden(true);
    ui->preview_button->setHidden(false);
    if (previewing_camera)
        previewing_camera->StopCapture();
    if (timer_id != 0) {
        killTimer(timer_id);
        timer_id = 0;
    }
}

void ConfigurationCamera::timerEvent(QTimerEvent* event) {
    if (event->timerId() != timer_id)
        return;
    if (!previewing_camera) {
        killTimer(timer_id);
        timer_id = 0;
        return;
    }
    std::vector<u16> frame{previewing_camera->ReceiveFrame()};
    int width{ui->preview_box->size().width()};
    int height{static_cast<int>(width * 0.75)};
    if (width != preview_width || height != preview_height) {
        stopPreviewing();
        return;
    }
    QImage image{width, height, QImage::Format::Format_RGB16};
    std::memcpy(image.bits(), frame.data(), width * height * sizeof(u16));
    ui->preview_box->setPixmap(QPixmap::fromImage(image));
}

void ConfigurationCamera::LoadConfiguration() {
    int index{GetSelectedCameraIndex()};
    for (int i{}; i < Implementations.size(); i++)
        if (Implementations[i] == camera_name[index])
            ui->image_source->setCurrentIndex(i);
    if (camera_name[index] == "image") {
        ui->camera_file->setDisabled(camera_config[index].empty());
        ui->toolButton->setDisabled(camera_config[index].empty());
        if (camera_config[index].empty())
            ui->camera_file->clear();
    }
    if (camera_name[index] == "qt") {
        ui->system_camera->setCurrentIndex(0);
        if (!camera_config[index].empty())
            ui->system_camera->setCurrentText(QString::fromStdString(camera_config[index]));
    } else
        ui->camera_file->setText(QString::fromStdString(camera_config[index]));
    ui->camera_flip->setCurrentIndex(camera_flip[index]);
    UpdateImageSourceUI();
}

void ConfigurationCamera::OnToolButtonClicked() {
    stopPreviewing();
    QList<QByteArray> types{QImageReader::supportedImageFormats()};
    QList<QString> temp_filters;
    for (const QByteArray& type : types)
        temp_filters << QString("*." + QString(type));
    QString filter{QString("Supported image files (%1)").arg(temp_filters.join(" "))};
    QString path{QFileDialog::getOpenFileName(this, QString("Open File"), ".", filter)};
    if (!path.isEmpty())
        ui->camera_file->setText(path);
}

void ConfigurationCamera::ApplyConfiguration() {
    RecordConfig();
    stopPreviewing();
    Settings::values.camera_name = camera_name;
    Settings::values.camera_config = camera_config;
    Settings::values.camera_flip = camera_flip;
}

ConfigurationCamera::CameraPosition ConfigurationCamera::GetCameraSelection() {
    switch (ui->camera_selection->currentIndex()) {
    case 0: // Front
        return CameraPosition::Front;
    case 1: // Rear
        if (ui->camera_mode->currentIndex() == 0)
            // Single (2D) mode
            return CameraPosition::RearBoth;
        else
            // Double (3D) mode
            return (ui->camera_position->currentIndex() == 0) ? CameraPosition::RearLeft
                                                              : CameraPosition::RearRight;
    default:
        LOG_ERROR(Frontend, "Unknown camera selection");
        return CameraPosition::Front;
    }
}

int ConfigurationCamera::GetSelectedCameraIndex() {
    auto pos{GetCameraSelection()};
    int camera_selection{static_cast<int>(pos)};
    if (pos == CameraPosition::RearBoth) // Single Mode
        camera_selection = 0;            // Either camera is the same, so we return RearRight
    return camera_selection;
}
