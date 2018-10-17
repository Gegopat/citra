// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cmath>
#include "citra/util/util.h"

QString ReadableByteSize(qulonglong size) {
    static const std::array<const char*, 6> units{{"B", "KiB", "MiB", "GiB", "TiB", "PiB"}};
    if (size == 0)
        return "0";
    int digit_groups{std::min<int>(static_cast<int>(std::log10(size) / std::log10(1024)),
                                   static_cast<int>(units.size()))};
    return QString("%L1 %2")
        .arg(size / std::pow(1024, digit_groups), 0, 'f', 1)
        .arg(units[digit_groups]);
}