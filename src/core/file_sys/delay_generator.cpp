// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "core/file_sys/delay_generator.h"

namespace FileSys {

DelayGenerator::~DelayGenerator() = default;

u64 DefaultDelayGenerator::GetReadDelayNs(std::size_t length) {
    // This is the delay measured for a romfs read.
    // For now we'll take that as a default
    constexpr u64 slope{94};
    constexpr u64 offset{582778};
    constexpr u64 minimum{663124};
    return std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
}

u64 DefaultDelayGenerator::GetOpenDelayNs() {
    // This is the delay measured for a romfs open.
    // For now we'll take that as a default
    return 9438006;
}

} // namespace FileSys
