// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>

namespace Core {
class System;
} // namespace Core

namespace Ui {
class ConfigurationDialog;
} // namespace Ui

class HotkeyRegistry;

class ConfigurationDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigurationDialog(QWidget* parent, HotkeyRegistry& hotkey_registry,
                                 Core::System& system);
    ~ConfigurationDialog() override;

    void ApplyConfiguration();

    bool restore_defaults_requested{};

private:
    void UpdateVisibleTabs();
    void PopulateSelectionList();

    std::unique_ptr<Ui::ConfigurationDialog> ui;
    Core::System& system;
    HotkeyRegistry& hotkey_registry;
};
