// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <unordered_map>
#include <QDialog>
#include "core/hle/applets/mii_selector.h"

namespace Ui {
class MiiSelectorDialog;
} // namespace Ui

class MiiSelectorDialog : public QDialog {
    Q_OBJECT

public:
    explicit MiiSelectorDialog(QWidget* parent, const HLE::Applets::MiiConfig& config,
                               HLE::Applets::MiiResult& result);
    ~MiiSelectorDialog();
    void ShowNoSelectableMiiCharacters(HLE::Applets::MiiResult& result);

private:
    std::unique_ptr<Ui::MiiSelectorDialog> ui;
    std::unordered_map<int, MiiData> miis;
};
