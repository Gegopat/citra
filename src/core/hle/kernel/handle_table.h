// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <cstddef>
#include <map>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Kernel {

enum KernelHandle : Handle {
    CurrentThread = 0xFFFF8000,
    CurrentProcess = 0xFFFF8001,
};

/**
 * This class allows the creation of Handles, which are references to objects that can be tested
 * for validity and looked up. Here they are used to pass references to kernel objects to/from the
 * emulated process.
 */
class HandleTable final : NonCopyable {
public:
    explicit HandleTable(KernelSystem& kernel);
    ~HandleTable();

    /**
     * Allocates a handle for the given object.
     * @return The created Handle
     */
    Handle Create(SharedPtr<Object> obj);

    /**
     * Returns a new handle that points to the same object as the passed in handle.
     * @return The duplicated Handle or one of the following errors:
     * - `ERR_INVALID_HANDLE`: an invalid handle was passed in.
     */
    ResultVal<Handle> Duplicate(Handle handle);

    /**
     * Closes a handle, removing it from the table and decreasing the object's ref-count.
     * @return `RESULT_SUCCESS` or one of the following errors:
     * - `ERR_INVALID_HANDLE`: an invalid handle was passed in.
     */
    ResultCode Close(Handle handle);

    /**
     * Looks up a handle.
     * @return Pointer to the looked-up object, or `nullptr` if the handle isn't valid.
     */
    SharedPtr<Object> GetGeneric(Handle handle) const;

    /**
     * Looks up a handle while verifying its type.
     * @return Pointer to the looked-up object, or `nullptr` if the handle isn't valid or its type
     * differs from the requested one.
     */
    template <class T>
    SharedPtr<T> Get(Handle handle) const {
        return DynamicObjectCast<T>(GetGeneric(handle));
    }

    /// Closes all handles held in this table.
    void Clear();

private:
    std::map<Handle, SharedPtr<Object>> objects{};
    std::atomic<u32> handle_counter{};

    KernelSystem& kernel;
};

} // namespace Kernel
