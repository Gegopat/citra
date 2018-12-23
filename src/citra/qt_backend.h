// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QObject>
#include <QString>
#include "citra/ui_settings.h"
#include "common/logging/backend.h"
#include "common/logging/text_formatter.h"

class QtBackend : public QObject, public Log::Backend {
    Q_OBJECT

public:
    const char* GetName() const override {
        return "qt";
    }

    void Write(const Log::Entry& entry) override {
        if (!UISettings::values.show_logging_window)
            return;
        QString color;
        switch (entry.log_level) {
        case Log::Level::Trace:
            color = "#808080";
            break;
        case Log::Level::Debug:
            color = "#008B8B";
            break;
        case Log::Level::Info:
            color = "#F5F5F5";
            break;
        case Log::Level::Warning:
            color = "#FFFF00";
            break;
        case Log::Level::Error:
            color = "#FF0000";
            break;
        case Log::Level::Critical:
            color = "#EA07D9";
            break;
        case Log::Level::Count:
            UNREACHABLE();
        }
        emit LineReady(
            QString("<font color='%1'>%2</font>")
                .arg(color, QString::fromStdString(Log::FormatLogMessage(entry)).toHtmlEscaped()));
    }

signals:
    void LineReady(const QString& line);
};
