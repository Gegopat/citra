// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include "common/common_types.h"
#include "core/cpu/common.h"

namespace Core {
class System;
} // namespace Core

namespace Memory {
struct PageTable;
} // namespace Memory

namespace Dynarmic::A32 {
struct Context;
class Jit;
} // namespace Dynarmic::A32

class UserCallbacks;

class ThreadContext final : NonCopyable {
public:
    ThreadContext();
    ~ThreadContext();

    void Reset();
    u32 GetCpuRegister(std::size_t index) const;
    void SetCpuRegister(std::size_t index, u32 value);
    void SetProgramCounter(u32 value);
    void SetStackPointer(u32 value);
    u32 GetCpsr() const;
    void SetCpsr(u32 value);
    u32 GetFpuRegister(std::size_t index) const;
    void SetFpuRegister(std::size_t index, u32 value);
    u32 GetFpscr() const;
    void SetFpscr(u32 value);
    u32 GetFpexc() const;
    void SetFpexc(u32 value);

private:
    friend class Cpu;

    Dynarmic::A32::Context* ctx;
    u32 fpexc{};
};

class Cpu {
public:
    explicit Cpu(Core::System& system);
    ~Cpu();

    void Run();

    void SetPC(u32 pc);
    u32 GetPC() const;
    u32 GetReg(int index) const;
    void SetReg(int index, u32 value);
    u32 GetVFPReg(int index) const;
    void SetVFPReg(int index, u32 value);
    u32 GetVFPSystemReg(VFPSystemRegister reg) const;
    void SetVFPSystemReg(VFPSystemRegister reg, u32 value);
    u32 GetCPSR() const;
    void SetCPSR(u32 cpsr);
    u32 GetCP15Register(CP15Register reg);
    void SetCP15Register(CP15Register reg, u32 value);

    std::unique_ptr<ThreadContext> NewContext() const;
    void SaveContext(const std::unique_ptr<ThreadContext>& arg);
    void LoadContext(const std::unique_ptr<ThreadContext>& arg);

    void PrepareReschedule();

    void ClearInstructionCache();
    void InvalidateCacheRange(u32 start_address, std::size_t length);
    void PageTableChanged();

    void SyncSettings();

private:
    friend class UserCallbacks;
    std::unique_ptr<UserCallbacks> cb;
    std::unique_ptr<Dynarmic::A32::Jit> MakeJit();

    Dynarmic::A32::Jit* jit;
    Memory::PageTable* current_page_table;
    std::map<Memory::PageTable*, std::unique_ptr<Dynarmic::A32::Jit>> jits;
    State state;
    Core::System& system;
};
