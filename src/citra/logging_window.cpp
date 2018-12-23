// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra/logging_window.h"
#include "common/logging/text_formatter.h"
#include "ui_logging_window.h"

LoggingWindow::LoggingWindow(QWidget* parent)
    : QDialog{parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint},
      ui{std::make_unique<Ui::LoggingWindow>()} {
    ui->setupUi(this);
    setWindowTitle("Logging Window");
}

LoggingWindow::~LoggingWindow() = default;

void LoggingWindow::Append(const QString& line) {
    ui->logs->append(line);
}

void LoggingWindow::Clear() {
    ui->logs->clear();
}
