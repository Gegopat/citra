// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>
#include <QFutureWatcher>
#include "citra/multiplayer/validation.h"

namespace Core {
class System;
} // namespace Core

namespace Ui {
class DirectConnect;
} // namespace Ui

class DirectConnectWindow : public QDialog {
    Q_OBJECT

public:
    explicit DirectConnectWindow(QWidget* parent, Core::System& system);
    ~DirectConnectWindow();

signals:
    /**
     * Signalled by this widget when it's closing itself and destroying any state such as
     * connections that it might have.
     */
    void Closed();

private slots:
    void OnConnection();

private:
    void Connect();
    void BeginConnecting();
    void EndConnecting();
    QFutureWatcher<void>* watcher;
    std::unique_ptr<Ui::DirectConnect> ui;
    Validation validation;
    Core::System& system;
};
