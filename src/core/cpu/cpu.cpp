// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/context.h>
#include "common/assert.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/cpu/cpu.h"
#include "core/cpu/cpu_cp15.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"
#include "core/settings.h"

static std::unordered_map<u64, u64> custom_ticks_map{{
    {0x000400000008C300, 570},   {0x000400000008C400, 570},   {0x000400000008C500, 570},
    {0x0004000000126A00, 570},   {0x0004000000126B00, 570},   {0x0004000200120C01, 570},
    {0x000400000F700E00, 18000}, {0x0004000000055D00, 17000}, {0x0004000000055E00, 17000},
    {0x000400000011C400, 17000}, {0x000400000011C500, 17000}, {0x0004000000164800, 17000},
    {0x0004000000175E00, 17000}, {0x00040000001B5000, 17000}, {0x00040000001B5100, 17000},
    {0x00040000001BC500, 27000}, {0x00040000001BC600, 27000}, {0x000400000016E100, 27000},
    {0x0004000000055F00, 27000}, {0x0004000000076500, 27000}, {0x0004000000076400, 27000},
    {0x00040000000D0000, 27000}, {0x0004000000126100, 6000},  {0x0004000000126300, 6000},
    {0x000400000011D700, 6000},
}};

class UserCallbacks final : public Dynarmic::A32::UserCallbacks {
public:
    explicit UserCallbacks(Cpu& parent, Core::System& system)
        : parent{parent}, system{system}, svc_context{system} {
        SyncSettings();
    }

    ~UserCallbacks() = default;

    std::uint8_t MemoryRead8(VAddr vaddr) override {
        return system.Memory().Read8(vaddr);
    }

    std::uint16_t MemoryRead16(VAddr vaddr) override {
        return system.Memory().Read16(vaddr);
    }

    std::uint32_t MemoryRead32(VAddr vaddr) override {
        return system.Memory().Read32(vaddr);
    }

    std::uint64_t MemoryRead64(VAddr vaddr) override {
        return system.Memory().Read64(vaddr);
    }

    void MemoryWrite8(VAddr vaddr, std::uint8_t value) override {
        system.Memory().Write8(vaddr, value);
    }

    void MemoryWrite16(VAddr vaddr, std::uint16_t value) override {
        system.Memory().Write16(vaddr, value);
    }

    void MemoryWrite32(VAddr vaddr, std::uint32_t value) override {
        system.Memory().Write32(vaddr, value);
    }

    void MemoryWrite64(VAddr vaddr, std::uint64_t value) override {
        system.Memory().Write64(vaddr, value);
    }

    void InterpreterFallback(VAddr pc, std::size_t num_instructions) override {
        ASSERT_MSG(false, "Interpreter fallback (pc={}, num_instructions={})", static_cast<u32>(pc),
                   num_instructions);
    }

    void CallSVC(std::uint32_t swi) override {
        svc_context.CallSVC(swi);
    }

    void ExceptionRaised(VAddr pc, Dynarmic::A32::Exception exception) override {
        ASSERT_MSG(false, "ExceptionRaised(exception={}, pc={:08X}, code={:08X})",
                   static_cast<std::size_t>(exception), pc, MemoryReadCode(pc));
    }

    void AddTicks(std::uint64_t ticks) override {
        system.CoreTiming().AddTicks(use_custom_ticks ? custom_ticks : ticks);
    }

    std::uint64_t GetTicksRemaining() override {
        s64 ticks{system.CoreTiming().GetDowncount()};
        return static_cast<u64>(ticks <= 0 ? 0 : ticks);
    }

    void SyncSettings() {
        custom_ticks = Settings::values.ticks;
        use_custom_ticks = Settings::values.ticks_mode != Settings::TicksMode::Accurate;
        switch (Settings::values.ticks_mode) {
        case Settings::TicksMode::Custom: {
            custom_ticks = Settings::values.ticks;
            use_custom_ticks = true;
            break;
        }
        case Settings::TicksMode::Auto: {
            u64 program_id{};
            system.GetProgramLoader().ReadProgramID(program_id);
            auto itr{custom_ticks_map.find(program_id)};
            if (itr != custom_ticks_map.end()) {
                custom_ticks = itr->second;
                use_custom_ticks = true;
            } else {
                custom_ticks = 0;
                use_custom_ticks = false;
            }
            break;
        }
        case Settings::TicksMode::Accurate: {
            custom_ticks = 0;
            use_custom_ticks = false;
            break;
        }
        }
    }

private:
    Cpu& parent;
    u64 custom_ticks{};
    bool use_custom_ticks{};
    Core::System& system;
    Kernel::SVCContext svc_context;
};

ThreadContext::ThreadContext() {
    ctx = new Dynarmic::A32::Context;
    Reset();
}

ThreadContext::~ThreadContext() = default;

void ThreadContext::Reset() {
    ctx->Regs() = {};
    ctx->SetCpsr(0);
    ctx->ExtRegs() = {};
    ctx->SetFpscr(0);
    fpexc = 0;
}

u32 ThreadContext::GetCpuRegister(std::size_t index) const {
    return ctx->Regs()[index];
}

void ThreadContext::SetCpuRegister(std::size_t index, u32 value) {
    ctx->Regs()[index] = value;
}

void ThreadContext::SetProgramCounter(u32 value) {
    return SetCpuRegister(15, value);
}

void ThreadContext::SetStackPointer(u32 value) {
    return SetCpuRegister(13, value);
}

u32 ThreadContext::GetCpsr() const {
    return ctx->Cpsr();
}

void ThreadContext::SetCpsr(u32 value) {
    ctx->SetCpsr(value);
}

u32 ThreadContext::GetFpuRegister(std::size_t index) const {
    return ctx->ExtRegs()[index];
}

void ThreadContext::SetFpuRegister(std::size_t index, u32 value) {
    ctx->ExtRegs()[index] = value;
}

u32 ThreadContext::GetFpscr() const {
    return ctx->Fpscr();
}

void ThreadContext::SetFpscr(u32 value) {
    ctx->SetFpscr(value);
}

u32 ThreadContext::GetFpexc() const {
    return fpexc;
}

void ThreadContext::SetFpexc(u32 value) {
    fpexc = value;
}

Cpu::Cpu(Core::System& system)
    : cb{std::make_unique<UserCallbacks>(*this, system)}, system{system} {
    PageTableChanged();
}

Cpu::~Cpu() = default;

void Cpu::Run() {
    ASSERT(system.Memory().GetCurrentPageTable() == current_page_table);
    jit->Run();
}

void Cpu::SetPC(u32 pc) {
    jit->Regs()[15] = pc;
}

u32 Cpu::GetPC() const {
    return jit->Regs()[15];
}

u32 Cpu::GetReg(int index) const {
    return jit->Regs()[index];
}

void Cpu::SetReg(int index, u32 value) {
    jit->Regs()[index] = value;
}

u32 Cpu::GetVFPReg(int index) const {
    return jit->ExtRegs()[index];
}

void Cpu::SetVFPReg(int index, u32 value) {
    jit->ExtRegs()[index] = value;
}

u32 Cpu::GetVFPSystemReg(VFPSystemRegister reg) const {
    if (reg == VFP_FPSCR)
        return jit->Fpscr();
    // Dynarmic doesn't implement and/or expose other VFP registers
    return state.vfp[reg];
}

void Cpu::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    if (reg == VFP_FPSCR)
        jit->SetFpscr(value);
    // Dynarmic doesn't implement and/or expose other VFP registers
    state.vfp[reg] = value;
}

u32 Cpu::GetCPSR() const {
    return jit->Cpsr();
}

void Cpu::SetCPSR(u32 cpsr) {
    jit->SetCpsr(cpsr);
}

u32 Cpu::GetCP15Register(CP15Register reg) {
    return state.cp15[reg];
}

void Cpu::SetCP15Register(CP15Register reg, u32 value) {
    state.cp15[reg] = value;
}

std::unique_ptr<ThreadContext> Cpu::NewContext() const {
    return std::make_unique<ThreadContext>();
}

void Cpu::SaveContext(const std::unique_ptr<ThreadContext>& arg) {
    auto ctx{dynamic_cast<ThreadContext*>(arg.get())};
    ASSERT(ctx);
    jit->SaveContext(*ctx->ctx);
    ctx->fpexc = state.vfp[VFP_FPEXC];
}

void Cpu::LoadContext(const std::unique_ptr<ThreadContext>& arg) {
    auto ctx{dynamic_cast<ThreadContext*>(arg.get())};
    ASSERT(ctx);
    jit->LoadContext(*ctx->ctx);
    state.vfp[VFP_FPEXC] = ctx->fpexc;
}

void Cpu::PrepareReschedule() {
    if (jit->IsExecuting())
        jit->HaltExecution();
}

void Cpu::InvalidateCacheRange(u32 start_address, std::size_t length) {
    jit->InvalidateCacheRange(start_address, length);
}

void Cpu::PageTableChanged() {
    current_page_table = system.Memory().GetCurrentPageTable();
    auto iter{jits.find(current_page_table)};
    if (iter != jits.end()) {
        jit = iter->second.get();
        return;
    }
    auto new_jit{MakeJit()};
    jit = new_jit.get();
    jits.emplace(current_page_table, std::move(new_jit));
}

void Cpu::SyncSettings() {
    cb->SyncSettings();
}

std::unique_ptr<Dynarmic::A32::Jit> Cpu::MakeJit() {
    Dynarmic::A32::UserConfig config;
    config.callbacks = cb.get();
    config.page_table = &current_page_table->pointers;
    config.coprocessors[15] = std::make_shared<CPUCP15>(state);
    config.define_unpredictable_behaviour = true;
    return std::make_unique<Dynarmic::A32::Jit>(config);
}
