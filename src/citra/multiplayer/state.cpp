// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include "citra/multiplayer/client_room.h"
#include "citra/multiplayer/direct_connect.h"
#include "citra/multiplayer/host_room.h"
#include "citra/multiplayer/lobby.h"
#include "citra/multiplayer/message.h"
#include "citra/multiplayer/state.h"
#include "citra/ui_settings.h"
#include "citra/util/clickable_label.h"
#include "common/announce_multiplayer_room.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_page.h"
#include "network/room.h"
#include "network/room_member.h"

MultiplayerState::MultiplayerState(QWidget* parent, QAction* leave_room, QAction* show_room,
                                   Core::System& system)
    : QWidget{parent}, leave_room{leave_room}, show_room{show_room}, system{system} {
    // Register the network structs to use in slots and signals
    qRegisterMetaType<Network::RoomMember::State>();
    qRegisterMetaType<Network::RoomMember::Error>();
    qRegisterMetaType<Common::WebResult>();
    auto& member{system.RoomMember()};
    state_callback_handle = member.BindOnStateChanged(
        [this](const Network::RoomMember::State& state) { emit NetworkStateChanged(state); });
    connect(this, &MultiplayerState::NetworkStateChanged, this,
            &MultiplayerState::OnNetworkStateChanged);
    error_callback_handle = member.BindOnError(
        [this](const Network::RoomMember::Error& error) { emit NetworkError(error); });
    connect(this, &MultiplayerState::NetworkError, this, &MultiplayerState::OnNetworkError);
    announce_multiplayer_session =
        std::make_shared<Core::AnnounceMultiplayerSession>(system.Room());
    announce_multiplayer_session->BindErrorCallback(
        [this](const Common::WebResult& result) { emit AnnounceFailed(result); });
    connect(this, &MultiplayerState::AnnounceFailed, this, &MultiplayerState::OnAnnounceFailed);
    status_icon = new ClickableLabel(this);
    status_icon->setPixmap(QIcon::fromTheme("disconnected").pixmap(16));
    connect(status_icon, &ClickableLabel::clicked, this, &MultiplayerState::OnOpenRoom);
    connect(static_cast<QApplication*>(QApplication::instance()), &QApplication::focusChanged, this,
            [this](QWidget*, QWidget* now) {
                if (client_room && client_room->isAncestorOf(now))
                    HideNotification();
            });
}

MultiplayerState::~MultiplayerState() {
    auto& member{system.RoomMember()};
    if (state_callback_handle)
        member.Unbind(state_callback_handle);
    if (error_callback_handle)
        member.Unbind(error_callback_handle);
}

void MultiplayerState::Close() {
    if (host_room)
        host_room->close();
    if (direct_connect)
        direct_connect->close();
    if (client_room)
        client_room->close();
    if (lobby)
        lobby->close();
}

void MultiplayerState::OnNetworkStateChanged(const Network::RoomMember::State& state) {
    LOG_DEBUG(Frontend, "Network State: {}", Network::GetStateStr(state));
    if (state == Network::RoomMember::State::Joined) {
        if (system.IsPoweredOn())
            system.Kernel().GetSharedPageHandler().SetMACAddress(
                system.RoomMember().GetMACAddress());
        OnOpenRoom();
        status_icon->setPixmap(QIcon::fromTheme("connected").pixmap(16));
        leave_room->setEnabled(true);
        show_room->setEnabled(true);
    } else {
        status_icon->setPixmap(QIcon::fromTheme("disconnected").pixmap(16));
        leave_room->setEnabled(false);
        show_room->setEnabled(false);
    }
    current_state = state;
}

void MultiplayerState::OnNetworkError(const Network::RoomMember::Error& error) {
    LOG_DEBUG(Frontend, "Network Error: {}", Network::GetErrorStr(error));
    switch (error) {
    case Network::RoomMember::Error::LostConnection:
        NetworkMessage::ShowError(NetworkMessage::LOST_CONNECTION);
        break;
    case Network::RoomMember::Error::HostKicked:
        if (client_room)
            client_room->Disconnect(false);
        NetworkMessage::ShowError(NetworkMessage::HOST_KICKED);
        break;
    case Network::RoomMember::Error::CouldNotConnect:
        NetworkMessage::ShowError(NetworkMessage::UNABLE_TO_CONNECT);
        break;
    case Network::RoomMember::Error::NameCollision:
        NetworkMessage::ShowError(NetworkMessage::NICKNAME_NOT_VALID_SERVER);
        break;
    case Network::RoomMember::Error::MACCollision:
        NetworkMessage::ShowError(NetworkMessage::MAC_COLLISION);
        break;
    case Network::RoomMember::Error::ConsoleIDCollision:
        NetworkMessage::ShowError(NetworkMessage::CONSOLE_ID_COLLISION);
        break;
    case Network::RoomMember::Error::RoomIsFull:
        NetworkMessage::ShowError(NetworkMessage::ROOM_IS_FULL);
        break;
    case Network::RoomMember::Error::WrongPassword:
        NetworkMessage::ShowError(NetworkMessage::WRONG_PASSWORD);
        break;
    case Network::RoomMember::Error::WrongVersion:
        NetworkMessage::ShowError(NetworkMessage::WRONG_VERSION);
        break;
    case Network::RoomMember::Error::HostBanned:
        if (client_room)
            client_room->Disconnect(false);
        NetworkMessage::ShowError(NetworkMessage::HOST_BANNED);
        break;
    case Network::RoomMember::Error::UnknownError:
        NetworkMessage::ShowError(NetworkMessage::UNABLE_TO_CONNECT);
        break;
    case Network::RoomMember::Error::PermissionDenied:
        NetworkMessage::ShowError(NetworkMessage::PERMISSION_DENIED);
        break;
    case Network::RoomMember::Error::NoSuchUser:
        NetworkMessage::ShowError(NetworkMessage::NO_SUCH_USER);
        break;
    }
}

void MultiplayerState::OnAnnounceFailed(const Common::WebResult& result) {
    announce_multiplayer_session->Stop();
    QMessageBox::warning(this, "Error",
                         QString("Failed to announce the room to the public lobby. Please report "
                                 "this issue now.\nDebug Message: ") +
                             QString::fromStdString(result.result_string),
                         QMessageBox::Ok);
}

void MultiplayerState::UpdateThemedIcons() {
    if (show_notification)
        status_icon->setPixmap(QIcon::fromTheme("connected_notification").pixmap(16));
    else if (current_state == Network::RoomMember::State::Joined)
        status_icon->setPixmap(QIcon::fromTheme("connected").pixmap(16));
    else
        status_icon->setPixmap(QIcon::fromTheme("disconnected").pixmap(16));
}

static void BringWidgetToFront(QWidget* widget) {
    widget->show();
    widget->activateWindow();
    widget->raise();
}

void MultiplayerState::OnViewLobby() {
    if (!lobby)
        lobby = new Lobby(this, announce_multiplayer_session, system);
    BringWidgetToFront(lobby);
}

void MultiplayerState::OnCreateRoom() {
    if (!host_room)
        host_room = new HostRoomWindow(this, announce_multiplayer_session, system);
    BringWidgetToFront(host_room);
}

bool MultiplayerState::OnCloseRoom(bool confirm) {
    if (!NetworkMessage::WarnCloseRoom(confirm))
        return false;
    auto& room{system.Room()};
    auto& member{system.RoomMember()};
    // If we're in a room, leave it
    member.Leave();
    LOG_DEBUG(Frontend, "Left the room (as a client)");
    // If we're hosting a room, also stop hosting
    if (!room.IsOpen())
        return true;
    // Save ban list
    UISettings::values.ban_list = std::move(room.GetBanList());
    room.Destroy();
    announce_multiplayer_session->Stop();
    LOG_DEBUG(Frontend, "Closed the room (as a server)");
    replies.clear();
    return true;
}

void MultiplayerState::OnOpenRoom() {
    auto& member{system.RoomMember()};
    if (member.IsConnected()) {
        if (!client_room) {
            client_room = new ClientRoomWindow(this, system);
            connect(client_room, &ClientRoomWindow::ShowNotification, this,
                    &MultiplayerState::ShowNotification);
        }
        const auto host{member.GetRoomInformation().creator};
        if (host.empty())
            client_room->SetModPerms(false);
        else
            client_room->SetModPerms(member.GetNickname() == host);
        BringWidgetToFront(client_room);
        return;
    }
    // If the user isn't a member of a room, show the lobby instead.
    // This is currently only used on the clickable label in the status bar
    OnViewLobby();
}

void MultiplayerState::OnDirectConnect() {
    if (!direct_connect)
        direct_connect = new DirectConnectWindow(this, system);
    BringWidgetToFront(direct_connect);
}

void MultiplayerState::ShowNotification() {
    if (client_room && client_room->isAncestorOf(QApplication::focusWidget()))
        return; // Don't show notification if the chat window currently has focus
    show_notification = true;
    QApplication::alert(nullptr);
    status_icon->setPixmap(QIcon::fromTheme("connected_notification").pixmap(16));
}

void MultiplayerState::HideNotification() {
    show_notification = false;
    status_icon->setPixmap(QIcon::fromTheme("connected").pixmap(16));
}
