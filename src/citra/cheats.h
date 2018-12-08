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
class CheatDialog;
} // namespace Ui

class CheatDialog : public QDialog {
    Q_OBJECT

public:
    explicit CheatDialog(Core::System& system, QWidget* parent = nullptr);
    ~CheatDialog() override;

private:
    void LoadCheats();

    std::unique_ptr<Ui::CheatDialog> ui;
    Core::System& system;

private slots:
    void OnCancel();
    void OnRowSelected(int row, int column);
    void OnCheckChanged(int state);
};
