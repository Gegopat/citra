// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "core/hle/service/gsp/gsp_lcd.h"

namespace Core {
class System;
} // namespace Core

namespace Service::GSP {

void InstallInterfaces(Core::System& system);

} // namespace Service::GSP
