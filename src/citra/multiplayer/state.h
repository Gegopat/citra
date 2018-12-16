// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>
#include <QWidget>

namespace Core {
class System;
} // namespace Core

class Lobby;
class HostRoomWindow;
class ClientRoomWindow;
class DirectConnectWindow;
class ClickableLabel;

class MultiplayerState : public QWidget {
    Q_OBJECT

public:
    using Replies = std::unordered_map<std::string, std::string>;

    explicit MultiplayerState(QWidget* parent, QAction* leave_room, QAction* show_room,
                              Core::System& system);
    ~MultiplayerState();

    /// Close all open multiplayer related dialogs
    void Close();

    ClickableLabel* GetStatusIcon() const {
        return status_icon;
    }

    void SetReplies(Replies replies) {
        this->replies = std::move(replies);
    }

    const Replies& GetReplies() {
        return replies;
    }

    ClientRoomWindow* GetRoomWindow() {
        return client_room;
    }

public slots:
    void OnNetworkStateChanged(const Network::RoomMember::State& state);
    void OnNetworkError(const Network::RoomMember::Error& error);
    void OnViewLobby();
    void OnCreateRoom();
    bool OnCloseRoom();
    void OnOpenRoom();
    void OnDirectConnect();
    void OnAnnounceFailed(const Common::WebResult&);
    void UpdateThemedIcons();
    void ShowNotification();
    void HideNotification();

signals:
    void NetworkStateChanged(const Network::RoomMember::State&);
    void NetworkError(const Network::RoomMember::Error&);
    void AnnounceFailed(const Common::WebResult&);

private:
    Lobby* lobby{};
    HostRoomWindow* host_room{};
    ClientRoomWindow* client_room{};
    DirectConnectWindow* direct_connect{};
    ClickableLabel* status_icon;
    QAction *leave_room, *show_room;
    Network::RoomMember::State current_state{Network::RoomMember::State::Uninitialized};
    bool has_mod_perms{}, show_notification{};
    Network::RoomMember::CallbackHandle<Network::RoomMember::State> state_callback_handle;
    Network::RoomMember::CallbackHandle<Network::RoomMember::Error> error_callback_handle;
    Replies replies;
    Core::System& system;
};

Q_DECLARE_METATYPE(Common::WebResult);
