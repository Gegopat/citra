// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "audio_core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
} // namespace Core

namespace Service::DSP {

class DSP_DSP final : public ServiceFramework<DSP_DSP> {
public:
    explicit DSP_DSP(Core::System& system);
    ~DSP_DSP();

    /// There are three types of interrupts
    static constexpr std::size_t NUM_INTERRUPT_TYPE{3};
    enum class InterruptType : u32 { Zero = 0, One = 1, Pipe = 2 };

    /// Actual service implementation only has 6 'slots' for interrupts.
    static constexpr std::size_t max_number_of_interrupt_events{6};

    /// Signal interrupt on pipe
    void SignalInterrupt(InterruptType type, AudioCore::DspPipe pipe);

private:
    void RecvData(Kernel::HLERequestContext& ctx);
    void RecvDataIsReady(Kernel::HLERequestContext& ctx);
    void SetSemaphore(Kernel::HLERequestContext& ctx);
    void ConvertProcessAddressFromDspDram(Kernel::HLERequestContext& ctx);
    void WriteProcessPipe(Kernel::HLERequestContext& ctx);
    void ReadPipe(Kernel::HLERequestContext& ctx);
    void GetPipeReadableSize(Kernel::HLERequestContext& ctx);
    void ReadPipeIfPossible(Kernel::HLERequestContext& ctx);
    void LoadComponent(Kernel::HLERequestContext& ctx);
    void UnloadComponent(Kernel::HLERequestContext& ctx);
    void FlushDataCache(Kernel::HLERequestContext& ctx);
    void InvalidateDataCache(Kernel::HLERequestContext& ctx);
    void RegisterInterruptEvents(Kernel::HLERequestContext& ctx);
    void GetSemaphoreEventHandle(Kernel::HLERequestContext& ctx);
    void SetSemaphoreMask(Kernel::HLERequestContext& ctx);
    void GetHeadphoneStatus(Kernel::HLERequestContext& ctx);
    void ForceHeadphoneOut(Kernel::HLERequestContext& ctx);
    void GetIsDspOccupied(Kernel::HLERequestContext& ctx);

    /// Returns the Interrupt Event for a given pipe
    Kernel::SharedPtr<Kernel::Event>& GetInterruptEvent(InterruptType type,
                                                        AudioCore::DspPipe pipe);
    /// Checks if we're trying to register more than 6 events
    bool HasTooManyEventsRegistered() const;

    u16 preset_semaphore{};

    Kernel::SharedPtr<Kernel::Event> semaphore_event;
    Kernel::SharedPtr<Kernel::Event> interrupt_zero;
    Kernel::SharedPtr<Kernel::Event> interrupt_one; // Currently unknown purpose

    /// Each DSP pipe has an associated interrupt
    std::array<Kernel::SharedPtr<Kernel::Event>, AudioCore::num_dsp_pipe> pipes{};

    Core::System& system;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::DSP
