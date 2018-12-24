// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QTableWidgetItem>
#include "citra/cheats.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "ui_cheats.h"

CheatDialog::CheatDialog(Core::System& system, QWidget* parent)
    : QDialog{parent}, ui{std::make_unique<Ui::CheatDialog>()}, system{system} {
    // Setup gui control settings
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    ui->tableCheats->setColumnWidth(0, 30);
    ui->tableCheats->setColumnWidth(2, 85);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->textCode->setEnabled(false);
    ui->textNotes->setEnabled(false);
    const auto id{fmt::format("{:016X}", system.Kernel().GetCurrentProcess()->codeset->program_id)};
    ui->labelID->setText(QString("ID: %1").arg(QString::fromStdString(id)));
    connect(ui->buttonClose, &QPushButton::released, this, &CheatDialog::OnCancel);
    connect(ui->tableCheats, &QTableWidget::cellClicked, this, &CheatDialog::OnRowSelected);
    LoadCheats();
}

CheatDialog::~CheatDialog() = default;

void CheatDialog::LoadCheats() {
    const auto& cheats{system.CheatEngine().GetCheats()};
    ui->tableCheats->setRowCount(static_cast<int>(cheats.size()));
    for (int i{}; i < static_cast<int>(cheats.size()); i++) {
        auto enabled{new QCheckBox()};
        enabled->setChecked(cheats[i]->IsEnabled());
        enabled->setStyleSheet("margin-left:7px;");
        ui->tableCheats->setItem(i, 0, new QTableWidgetItem());
        ui->tableCheats->setCellWidget(i, 0, enabled);
        ui->tableCheats->setItem(
            i, 1, new QTableWidgetItem(QString::fromStdString(cheats[i]->GetName())));
        ui->tableCheats->setItem(
            i, 2, new QTableWidgetItem(QString::fromStdString(cheats[i]->GetType())));
        enabled->setProperty("row", i);
        connect(enabled, &QCheckBox::stateChanged, this, &CheatDialog::OnCheckChanged);
    }
}

void CheatDialog::OnCancel() {
    close();
}

void CheatDialog::OnRowSelected(int row, int column) {
    ui->textCode->setEnabled(true);
    ui->textNotes->setEnabled(true);
    const auto& current_cheat{system.CheatEngine().GetCheats()[row]};
    ui->textNotes->setPlainText(QString::fromStdString(current_cheat->GetComments()));
    ui->textCode->setPlainText(QString::fromStdString(current_cheat->ToString()));
}

void CheatDialog::OnCheckChanged(int state) {
    system.CheatEngine()
        .GetCheats()[qobject_cast<QCheckBox*>(sender())->property("row").toInt()]
        ->SetEnabled(state);
}
