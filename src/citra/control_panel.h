// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>

namespace Core {
class System;
} // namespace Core

namespace Ui {
class ControlPanel;
} // namespace Ui

class ControlPanel : public QDialog {
    Q_OBJECT

public:
    explicit ControlPanel(Core::System& system, QWidget* parent = nullptr);
    ~ControlPanel() override;

    void Update3D();
    void UpdateNetwork();

private:
    std::unique_ptr<Ui::ControlPanel> ui;
};
