// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QMessageBox>
#include "citra/configuration/hotkeys.h"
#include "citra/hotkeys.h"
#include "citra/util/sequence_dialog/sequence_dialog.h"
#include "core/settings.h"
#include "ui_hotkeys.h"

ConfigurationHotkeys::ConfigurationHotkeys(QWidget* parent)
    : QWidget{parent}, ui{std::make_unique<Ui::ConfigurationHotkeys>()} {
    ui->setupUi(this);
    setFocusPolicy(Qt::ClickFocus);
    model = new QStandardItemModel(this);
    model->setColumnCount(3);
    model->setHorizontalHeaderLabels({"Action", "Hotkey", "Context"});
    ui->hotkey_list->setSelectionMode(QTreeView::SingleSelection);
    connect(ui->hotkey_list, &QTreeView::doubleClicked, this, &ConfigurationHotkeys::Configure);
    ui->hotkey_list->setModel(model);
    // TODO: Make context configurable as well (hiding the column for now)
    ui->hotkey_list->hideColumn(2);
    ui->hotkey_list->setColumnWidth(0, 200);
    ui->hotkey_list->resizeColumnToContents(1);
    ui->hotkey_list->setEditTriggers(QTreeView::NoEditTriggers);
}

ConfigurationHotkeys::~ConfigurationHotkeys() {}

void ConfigurationHotkeys::EmitHotkeysChanged() {
    emit HotkeysChanged(GetUsedKeyList());
}

QList<QKeySequence> ConfigurationHotkeys::GetUsedKeyList() {
    QList<QKeySequence> list;
    for (int r = 0; r < model->rowCount(); r++) {
        QStandardItem* parent = model->item(r, 0);
        for (int r2 = 0; r2 < parent->rowCount(); r2++) {
            QStandardItem* keyseq = parent->child(r2, 1);
            list << QKeySequence::fromString(keyseq->text(), QKeySequence::NativeText);
        }
    }
    return list;
}

void ConfigurationHotkeys::Populate(const HotkeyRegistry& registry) {
    for (const auto& group : registry.hotkey_groups) {
        auto parent_item{new QStandardItem(group.first)};
        parent_item->setEditable(false);
        for (const auto& hotkey : group.second) {
            auto action{new QStandardItem(hotkey.first)};
            auto keyseq{new QStandardItem(hotkey.second.keyseq.toString(QKeySequence::NativeText))};
            action->setEditable(false);
            keyseq->setEditable(false);
            parent_item->appendRow({action, keyseq});
        }
        model->appendRow(parent_item);
    }
    ui->hotkey_list->expandAll();
}

void ConfigurationHotkeys::OnInputKeysChanged(QList<QKeySequence> new_key_list) {
    input_keys_list = new_key_list;
}

void ConfigurationHotkeys::Configure(QModelIndex index) {
    if (index.parent() == QModelIndex())
        return;
    index = index.sibling(index.row(), 1);
    auto model{ui->hotkey_list->model()};
    auto previous_key{model->data(index)};
    auto hotkey_dialog{new SequenceDialog};
    int return_code{hotkey_dialog->exec()};
    auto key_sequence{hotkey_dialog->GetSequence()};
    if (return_code == QDialog::Rejected || key_sequence.isEmpty())
        return;
    if (IsUsedKey(key_sequence) &&
        key_sequence != QKeySequence{previous_key.toString(), QKeySequence::NativeText}) {
        model->setData(index, previous_key);
        QMessageBox::critical(this, "Error in inputted key",
                              "You're using a key that's already bound.");
    } else {
        model->setData(index, key_sequence.toString(QKeySequence::NativeText));
        EmitHotkeysChanged();
    }
}

bool ConfigurationHotkeys::IsUsedKey(QKeySequence key_sequence) {
    return input_keys_list.contains(key_sequence) || GetUsedKeyList().contains(key_sequence);
}

void ConfigurationHotkeys::ApplyConfiguration(HotkeyRegistry& registry) {
    for (int key_id{}; key_id < model->rowCount(); key_id++) {
        auto parent{model->item(key_id, 0)};
        for (int key_column_id = 0; key_column_id < parent->rowCount(); key_column_id++) {
            auto action{parent->child(key_column_id, 0)};
            auto keyseq{parent->child(key_column_id, 1)};
            for (auto key_iterator{registry.hotkey_groups.begin()};
                 key_iterator != registry.hotkey_groups.end(); ++key_iterator) {
                if (key_iterator->first == parent->text())
                    for (auto it2{key_iterator->second.begin()}; it2 != key_iterator->second.end();
                         ++it2)
                        if (it2->first == action->text())
                            it2->second.keyseq = QKeySequence{keyseq->text()};
            }
        }
    }
    registry.SaveHotkeys();
}