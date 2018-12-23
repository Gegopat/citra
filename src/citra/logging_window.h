// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>
#include <QString>

namespace Ui {
class LoggingWindow;
} // namespace Ui

class LoggingWindow : public QDialog {
    Q_OBJECT

public:
    explicit LoggingWindow(QWidget* parent = nullptr);
    ~LoggingWindow() override;

    void Append(const QString& line);
    void Clear();

private:
    std::unique_ptr<Ui::LoggingWindow> ui;
};
