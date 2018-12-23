// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QString>

/// Convert a size in bytes into a readable format (KiB, MiB, etc.)
QString ReadableByteSize(qulonglong size);
