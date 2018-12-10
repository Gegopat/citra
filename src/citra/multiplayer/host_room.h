// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVariant>
#include "citra/multiplayer/chat_room.h"
#include "citra/multiplayer/validation.h"

namespace Ui {
class HostRoom;
} // namespace Ui

namespace Core {
class System;
} // namespace Core

class ConnectionError;
class ComboBoxProxyModel;

class ChatMessage;

class HostRoomWindow : public QDialog {
    Q_OBJECT

public:
    explicit HostRoomWindow(QWidget* parent, Core::System& system);
    ~HostRoomWindow();

private slots:
    /**
     * Handler for connection status changes. Launches the chat window if successful or
     * displays an error
     */
    void OnConnection();

private:
    void Host();
    void AddReply();
    void RemoveReply();
    void UpdateReplies();

    std::unique_ptr<Ui::HostRoom> ui;
    Validation validation;
    Core::System& system;
};
