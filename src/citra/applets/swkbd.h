// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>
#include "core/hle/applets/swkbd.h"

namespace Ui {
class SoftwareKeyboardDialog;
} // namespace Ui

class SoftwareKeyboardDialog : public QDialog {
    Q_OBJECT

public:
    explicit SoftwareKeyboardDialog(QWidget* parent, HLE::Applets::SoftwareKeyboardConfig& config,
                                    std::u16string& text);
    ~SoftwareKeyboardDialog();

private:
    std::unique_ptr<Ui::SoftwareKeyboardDialog> ui;
};
