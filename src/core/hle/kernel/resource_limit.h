// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"

namespace Kernel {

enum class ResourceLimitCategory : u8 {
    Program = 0,
    SystemApplet = 1,
    LibraryApplet = 2,
    Other = 3
};

enum ResourceTypes {
    PRIORITY = 0,
    COMMIT = 1,
    THREAD = 2,
    EVENT = 3,
    MUTEX = 4,
    SEMAPHORE = 5,
    TIMER = 6,
    SHARED_MEMORY = 7,
    ADDRESS_ARBITER = 8,
    CPU_TIME = 9,
};

class ResourceLimit final : public Object {
public:
    /**
     * Creates a resource limit object.
     */
    static SharedPtr<ResourceLimit> Create(KernelSystem& kernel, std::string name = "Unknown");

    std::string GetTypeName() const override {
        return "ResourceLimit";
    }

    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE{HandleType::ResourceLimit};
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Gets the current value for the specified resource.
     * @param resource Requested resource type
     * @returns The current value of the resource type
     */
    s32 GetCurrentResourceValue(u32 resource) const;

    /**
     * Gets the max value for the specified resource.
     * @param resource Requested resource type
     * @returns The max value of the resource type
     */
    u32 GetMaxResourceValue(u32 resource) const;

    /// Name of resource limit object.
    std::string name;

    /// Max thread priority that a process in this category can create
    s32 max_priority{};

    /// Max memory that processes in this category can use
    s32 max_commit{};

    ///< Max number of objects that can be collectively created by the processes in this category
    s32 max_threads{};
    s32 max_events{};
    s32 max_mutexes{};
    s32 max_semaphores{};
    s32 max_timers{};
    s32 max_shared_mems{};
    s32 max_address_arbiters{};

    /// Max CPU time that the processes in this category can utilize
    s32 max_cpu_time{};

    // TODO: Increment these in their respective Kernel::T::Create functions, keeping in mind
    // that Program resource limits shouldn't be affected by the objects created by service
    // modules.
    // Currently we have no way of distinguishing if a Create was called by the running program,
    // or by a service module. Approach this once we have separated the service modules into their
    // own processes

    /// Current memory that the processes in this category are using
    s32 current_commit{};

    ///< Current number of objects among all processes in this category
    s32 current_threads{};
    s32 current_events{};
    s32 current_mutexes{};
    s32 current_semaphores{};
    s32 current_timers{};
    s32 current_shared_mems{};
    s32 current_address_arbiters{};

    /// Current CPU time that the processes in this category are utilizing
    s32 current_cpu_time{};

private:
    explicit ResourceLimit(KernelSystem& kernel);
    ~ResourceLimit() override;
};

class ResourceLimitList {
public:
    explicit ResourceLimitList(KernelSystem& kernel);
    ~ResourceLimitList();

    /**
     * Retrieves the resource limit associated with the specified resource limit category.
     * @param category The resource limit category
     * @returns The resource limit associated with the category
     */
    SharedPtr<ResourceLimit> GetForCategory(ResourceLimitCategory category);

private:
    std::array<SharedPtr<ResourceLimit>, 4> resource_limits;
};

} // namespace Kernel
