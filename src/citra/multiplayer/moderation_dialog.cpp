// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QStandardItem>
#include <QStandardItemModel>
#include "citra/multiplayer/moderation_dialog.h"
#include "ui_moderation_dialog.h"

namespace Column {

enum {
    Subject,
    Type,
    Count,
};

} // namespace Column

ModerationDialog::ModerationDialog(Network::RoomMember& member, QWidget* parent)
    : QDialog{parent}, member{member}, ui{std::make_unique<Ui::ModerationDialog>()} {
    ui->setupUi(this);
    qRegisterMetaType<Network::Room::BanList>();
    callback_handle_status_message = member.BindOnStatusMessageReceived(
        [this](const Network::StatusMessageEntry& status_message) {
            emit StatusMessageReceived(status_message);
        });
    connect(this, &ModerationDialog::StatusMessageReceived, this,
            &ModerationDialog::OnStatusMessageReceived);
    callback_handle_ban_list = member.BindOnBanListReceived(
        [this](const Network::Room::BanList& ban_list) { emit BanListReceived(ban_list); });
    connect(this, &ModerationDialog::BanListReceived, this, &ModerationDialog::PopulateBanList);
    // Initialize the UI
    model = new QStandardItemModel(ui->ban_list_view);
    model->insertColumns(0, Column::Count);
    model->setHeaderData(Column::Subject, Qt::Horizontal, "Subject");
    model->setHeaderData(Column::Type, Qt::Horizontal, "Type");
    ui->ban_list_view->setModel(model);
    // Load the ban list in background
    LoadBanList();
    connect(ui->refresh, &QPushButton::clicked, this, [this] { LoadBanList(); });
    connect(ui->unban, &QPushButton::clicked, this, [this] {
        auto index{ui->ban_list_view->currentIndex()};
        SendUnbanRequest(model->item(index.row(), 0)->text());
    });
    connect(ui->ban_list_view, &QTreeView::clicked, [this] { ui->unban->setEnabled(true); });
}

ModerationDialog::~ModerationDialog() {
    if (callback_handle_status_message)
        member.Unbind(callback_handle_status_message);
    if (callback_handle_ban_list)
        member.Unbind(callback_handle_ban_list);
}

void ModerationDialog::LoadBanList() {
    ui->refresh->setEnabled(false);
    ui->refresh->setText("Refreshing");
    ui->unban->setEnabled(false);
    member.RequestBanList();
}

void ModerationDialog::PopulateBanList(const Network::Room::BanList& ban_list) {
    model->removeRows(0, model->rowCount());
    for (const auto& ip : ban_list) {
        auto subject_item{new QStandardItem(QString::fromStdString(ip))};
        auto type_item{new QStandardItem("IP Address")};
        model->invisibleRootItem()->appendRow({subject_item, type_item});
    }
    for (int i{}; i < Column::Count - 1; ++i)
        ui->ban_list_view->resizeColumnToContents(i);
    ui->refresh->setEnabled(true);
    ui->refresh->setText("Refresh");
    ui->unban->setEnabled(false);
}

void ModerationDialog::SendUnbanRequest(const QString& subject) {
    member.SendModerationRequest(Network::IdModUnban, subject.toStdString());
}

void ModerationDialog::OnStatusMessageReceived(const Network::StatusMessageEntry& status_message) {
    if (status_message.type != Network::IdMemberBanned &&
        status_message.type != Network::IdAddressUnbanned)
        return;
    // Update the ban list for ban/unban
    LoadBanList();
}
