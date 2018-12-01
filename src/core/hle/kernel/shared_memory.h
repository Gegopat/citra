// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/process.h"
#include "core/hle/result.h"

namespace Kernel {

class SharedMemory final : public Object {
public:
    std::string GetTypeName() const override {
        return "SharedMemory";
    }

    std::string GetName() const override {
        return name;
    }

    void SetName(std::string name) {
        this->name = std::move(name);
    }

    static constexpr HandleType HANDLE_TYPE{HandleType::SharedMemory};
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /// Gets the size of the underlying memory block in bytes.
    u64 GetSize() const {
        return size;
    }

    /// Gets the linear heap physical offset
    u64 GetLinearHeapPhysicalOffset() const {
        return linear_heap_phys_offset;
    }

    /**
     * Converts the specified MemoryPermission into the equivalent VMAPermission.
     * @param permission The MemoryPermission to convert.
     */
    static VMAPermission ConvertPermissions(MemoryPermission permission);

    /**
     * Maps a shared memory block to an address in the target process' address space
     * @param target_process Process on which to map the memory block.
     * @param address Address in system memory to map shared memory block to
     * @param permissions Memory block map permissions (specified by SVC field)
     * @param other_permissions Memory block map other permissions (specified by SVC field)
     */
    ResultCode Map(Process& target_process, VAddr address, MemoryPermission permissions,
                   MemoryPermission other_permissions);

    /**
     * Unmaps a shared memory block from the specified address in system memory
     * @param target_process Process from which to unmap the memory block.
     * @param address Address in system memory where the shared memory block is mapped
     * @return Result code of the unmap operation
     */
    ResultCode Unmap(Process& target_process, VAddr address);

    /**
     * Gets a pointer to the shared memory block
     * @param offset Offset from the start of the shared memory block to get pointer
     * @return A pointer to the shared memory block from the specified offset
     */
    u8* GetPointer(u32 offset = 0);

    /**
     * Gets a constant pointer to the shared memory block
     * @param offset Offset from the start of the shared memory block to get pointer
     * @return A constant pointer to the shared memory block from the specified offset
     */
    const u8* GetPointer(u32 offset = 0) const;

private:
    explicit SharedMemory(KernelSystem& kernel);
    ~SharedMemory() override;

    PAddr linear_heap_phys_offset{}; ///< Offset in FCRAM of the shared memory block in the linear
                                     /// heap if no address was specified during creation.

    std::vector<std::pair<u8*, u32>>
        backing_blocks; ///< Backing memory for this shared memory block.

    u32 size{}; ///< Size of the memory block. Page-aligned.

    MemoryPermission
        permissions{}; ///< Permission restrictions applied to the process which created the block.

    MemoryPermission other_permissions{}; ///< Permission restrictions applied to other processes
                                          /// mapping the block.

    SharedPtr<Process> owner_process; ///< Process that created this shared memory block.

    VAddr base_address{}; ///< Address of shared memory block in the owner process if specified.

    std::string name; ///< Name of shared memory object.

    MemoryRegionInfo::IntervalSet holding_memory;

    friend class KernelSystem;
    KernelSystem& kernel;
};

} // namespace Kernel
