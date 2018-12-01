// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

HandleTable::HandleTable(KernelSystem& kernel) : kernel{kernel} {
    Clear();
}

HandleTable::~HandleTable() = default;

Handle HandleTable::Create(SharedPtr<Object> obj) {
    DEBUG_ASSERT(obj);
    return objects.emplace(++handle_counter, std::move(obj)).first->first;
}

ResultVal<Handle> HandleTable::Duplicate(Handle handle) {
    auto object{GetGeneric(handle)};
    if (!object) {
        LOG_ERROR(Kernel, "Tried to duplicate invalid handle {:08X}", handle);
        return ERR_INVALID_HANDLE;
    }
    return MakeResult<Handle>(Create(std::move(object)));
}

ResultCode HandleTable::Close(Handle handle) {
    if (objects.find(handle) == objects.end())
        return ERR_INVALID_HANDLE;
    objects.erase(handle);
    return RESULT_SUCCESS;
}

SharedPtr<Object> HandleTable::GetGeneric(Handle handle) const {
    if (handle == CurrentThread)
        return kernel.GetThreadManager().GetCurrentThread();
    else if (handle == CurrentProcess)
        return kernel.GetCurrentProcess();
    auto itr{objects.find(handle)};
    return itr == objects.end() ? nullptr : itr->second;
}

void HandleTable::Clear() {
    objects.clear();
}

} // namespace Kernel
