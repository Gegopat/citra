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
    ~ConfigurationHotkeys();

    void ApplyConfiguration(HotkeyRegistry& registry) void EmitHotkeysChanged();
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
    QList<QKeySequence> input_keys_list;
    QStandardItemModel* model;
};
