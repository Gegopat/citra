// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_set>
#include <QDialog>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVariant>
#include "network/room.h"
#include "network/room_member.h"

namespace Core {
class System;
} // namespace Core

namespace Ui {
class ChatRoom;
} // namespace Ui

class ConnectionError;
class ComboBoxProxyModel;
class ChatMessage;

class ChatRoom : public QWidget {
    Q_OBJECT

public:
    explicit ChatRoom(QWidget* parent);
    ~ChatRoom();

    void SetMemberList(const Network::RoomMember::MemberList& member_list);
    void Clear();
    void AppendStatusMessage(const QString& msg);
    bool Send(QString msg);
    void HandleNewMessage(const QString& msg);

    void SetModPerms(bool is_mod);

public slots:
    void OnChatReceive(const Network::ChatEntry&);
    void OnStatusMessageReceive(const Network::StatusMessageEntry&);
    void OnSendChat();
    void OnChatTextChanged();
    void PopupContextMenu(const QPoint& menu_location);
    void Disable();
    void Enable();

signals:
    void ChatReceived(const Network::ChatEntry&);
    void StatusMessageReceived(const Network::StatusMessageEntry&);
    void Pinged();

private:
    void AppendChatMessage(const QString&);
    bool ValidateMessage(const std::string&);
    void SendModerationRequest(Network::RoomMessageTypes type, const std::string& nickname);
    void UpdateIconDisplay();

    bool has_mod_perms{};
    QStandardItemModel* member_list;
    std::unique_ptr<Ui::ChatRoom> ui;
    std::unordered_set<std::string> block_list;
    Core::System& system;
};

Q_DECLARE_METATYPE(Network::ChatEntry);
Q_DECLARE_METATYPE(Network::StatusMessageEntry);
Q_DECLARE_METATYPE(Network::RoomInformation);
Q_DECLARE_METATYPE(Network::RoomMember::State);
Q_DECLARE_METATYPE(Network::RoomMember::Error);
