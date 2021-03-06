// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Memory {
class MemorySystem;
} // namespace Memory

namespace Service::Y2R {
struct ConversionConfiguration;
} // namespace Service::Y2R

namespace HW::Y2R {

void PerformConversion(Service::Y2R::ConversionConfiguration& cvt, Memory::MemorySystem& memory);

} // namespace HW::Y2R
