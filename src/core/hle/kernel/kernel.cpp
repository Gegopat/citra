// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/config_mem.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"
#include "core/hle/shared_page.h"
#include "core/settings.h"

namespace Kernel {

std::atomic<u32> Object::next_object_id{};

/// Initialize the kernel
KernelSystem::KernelSystem(u32 system_mode) {
    ConfigMem::Init();

    Kernel::MemoryInit(system_mode);

    Kernel::ResourceLimitsInit();
    Kernel::ThreadingInit();
    Kernel::TimersInit();

    Object::next_object_id = 0;
    Process::next_process_id = 0;
}

/// Shutdown the kernel
KernelSystem::~KernelSystem() {
    g_handle_table.Clear(); // Free all kernel objects

    Kernel::ThreadingShutdown();
    g_current_process = nullptr;

    Kernel::TimersShutdown();
    Kernel::ResourceLimitsShutdown();
    Kernel::MemoryShutdown();
}

} // namespace Kernel
