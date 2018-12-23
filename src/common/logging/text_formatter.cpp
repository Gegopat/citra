// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/backend.h"
#include "common/logging/text_formatter.h"

namespace Log {

std::string FormatLogMessage(const Entry& entry) {
    auto time_seconds{static_cast<unsigned>(entry.timestamp.count() / 1000000)};
    auto time_fractional{static_cast<unsigned>(entry.timestamp.count() % 1000000)};
    auto class_name{GetLogClassName(entry.log_class)};
    auto level_name{GetLevelName(entry.log_level)};
    return fmt::format("[{:d}.{:d}] {} <{}> {}:{}:{}: {}", time_seconds, time_fractional,
                       class_name, level_name, entry.filename, entry.function, entry.line_num,
                       entry.message);
}

} // namespace Log
