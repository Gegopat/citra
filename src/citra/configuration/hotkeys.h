// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QStandardItemModel>
#include <QWidget>
#include "common/param_package.h"
#include "core/settings.h"

namespace Ui {
class ConfigurationHotkeys;
} // namespace Ui

class HotkeyRegistry;

class ConfigurationHotkeys : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurationHotkeys(QWidget* parent = nullptr);
    ~ConfigurationHotkeys() override;

    void ApplyConfiguration(HotkeyRegistry& registry);
    void EmitHotkeysChanged();

    /**
     * Populates the hotkey list widget using data from the provided registry.
     * Called everytime the Configure dialog is opened.
     * @param registry The HotkeyRegistry whose data is used to populate the list.
     */
    void Populate(const HotkeyRegistry& registry);

public slots:
    void OnInputKeysChanged(QList<QKeySequence> new_key_list);

signals:
    void HotkeysChanged(QList<QKeySequence> new_key_list);

private:
    void Configure(QModelIndex index);
    bool IsUsedKey(QKeySequence key_sequence);
    QList<QKeySequence> GetUsedKeyList();

    std::unique_ptr<Ui::ConfigurationHotkeys> ui;

    /**
     * List of keyboard keys currently registered to any of the 3DS inputs.
     * These can't be bound to any hotkey.
     * Synchronised with ConfigureInput via signal-slot.
     */
    QList<QKeySequence> input_keys_list;

    QStandardItemModel* model;
};
