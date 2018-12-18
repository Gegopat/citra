// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "citra/multiplayer/chat_room.h"

namespace Ui {
class ClientRoom;
} // namespace Ui

class ClientRoomWindow : public QDialog {
    Q_OBJECT

public:
    explicit ClientRoomWindow(QWidget* parent, Core::System& system);
    ~ClientRoomWindow();

    void Disconnect(bool confirm = true);
    void SetModPerms(bool is_mod);

    Core::System& system;

signals:
    void RoomInformationChanged(const Network::RoomInformation&);
    void StateChanged(const Network::RoomMember::State&);
    void ShowNotification();

private:
    void OnRoomUpdate(const Network::RoomInformation&);
    void OnStateChange(const Network::RoomMember::State&);
    void UpdateView();

    QStandardItemModel* member_list;
    std::unique_ptr<Ui::ClientRoom> ui;
};
