// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QComboBox>
#include <QFuture>
#include <QIntValidator>
#include <QRegExpValidator>
#include <QString>
#include <QtConcurrent/QtConcurrentRun>
#include "citra/main.h"
#include "citra/multiplayer/client_room.h"
#include "citra/multiplayer/direct_connect.h"
#include "citra/multiplayer/message.h"
#include "citra/multiplayer/state.h"
#include "citra/multiplayer/validation.h"
#include "citra/ui_settings.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/settings.h"
#include "network/room.h"
#include "ui_direct_connect.h"

enum class ConnectionType : u8 { TraversalServer, IP };

DirectConnectWindow::DirectConnectWindow(QWidget* parent, Core::System& system)
    : QDialog{parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint},
      ui{std::make_unique<Ui::DirectConnect>()}, system{system} {
    ui->setupUi(this);
    // Setup the watcher for background connections
    watcher = new QFutureWatcher<void>;
    connect(watcher, &QFutureWatcher<void>::finished, this, &DirectConnectWindow::OnConnection);
    ui->nickname->setValidator(validation.GetNickname());
    ui->nickname->setText(UISettings::values.nickname);
    ui->ip->setValidator(validation.GetIP());
    ui->ip->setText(UISettings::values.ip);
    ui->port->setValidator(validation.GetPort());
    ui->port->setText(UISettings::values.port);
    // TODO: Show or hide the connection options based on the current value of the combo
    // box. Add this back in when the traversal server support is added.
    connect(ui->connect, &QPushButton::released, this, &DirectConnectWindow::Connect);
}

DirectConnectWindow::~DirectConnectWindow() = default;

void DirectConnectWindow::Connect() {
    if (!ui->nickname->hasAcceptableInput()) {
        NetworkMessage::ShowError(NetworkMessage::NICKNAME_NOT_VALID);
        return;
    }
    // Prevent the user from trying to join a room while they are already joining.
    auto& member{system.RoomMember()};
    if (member.GetState() == Network::RoomMember::State::Joining)
        return;
    else if (member.GetState() == Network::RoomMember::State::Joined)
        // And ask if they want to leave the room if they are already in one.
        if (!NetworkMessage::WarnDisconnect())
            return;
    switch (static_cast<ConnectionType>(ui->connection_type->currentIndex())) {
    case ConnectionType::TraversalServer:
        break;
    case ConnectionType::IP:
        if (!ui->ip->hasAcceptableInput()) {
            NetworkMessage::ShowError(NetworkMessage::IP_ADDRESS_NOT_VALID);
            return;
        }
        if (!ui->port->hasAcceptableInput()) {
            NetworkMessage::ShowError(NetworkMessage::PORT_NOT_VALID);
            return;
        }
        break;
    }
    // Store settings
    UISettings::values.nickname = ui->nickname->text();
    UISettings::values.ip = ui->ip->text();
    UISettings::values.port = (ui->port->isModified() && !ui->port->text().isEmpty())
                                  ? ui->port->text()
                                  : UISettings::values.port;
    // Attempt to connect in a different thread
    auto f{QtConcurrent::run([&] {
        auto port{UISettings::values.port.toUInt()};
        system.RoomMember().Join(ui->nickname->text().toStdString(),
                                 Service::CFG::GetConsoleID(system),
                                 ui->ip->text().toStdString().c_str(), port, BroadcastMac,
                                 ui->password->text().toStdString().c_str());
    })};
    watcher->setFuture(f);
    // And disable widgets and display a connecting while we wait
    BeginConnecting();
}

void DirectConnectWindow::BeginConnecting() {
    ui->connect->setEnabled(false);
    ui->connect->setText("Connecting");
}

void DirectConnectWindow::EndConnecting() {
    ui->connect->setEnabled(true);
    ui->connect->setText("Connect");
}

void DirectConnectWindow::OnConnection() {
    EndConnecting();
    if (system.RoomMember().GetState() == Network::RoomMember::State::Joined)
        close();
}
