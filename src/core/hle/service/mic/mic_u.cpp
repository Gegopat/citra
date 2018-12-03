// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <SDL.h>
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/mic/mic_u.h"
#include "core/settings.h"

namespace Service::MIC {

enum class Encoding : u8 {
    PCM8 = 0,
    PCM16 = 1,
    PCM8Signed = 2,
    PCM16Signed = 3,
};

enum class SampleRate : u8 {
    SampleRate32730 = 0,
    SampleRate16360 = 1,
    SampleRate10910 = 2,
    SampleRate8180 = 3
};

struct MIC_U::Impl {
    explicit Impl(Core::System& system) {
        buffer_full_event =
            system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "mic:u Buffer Full Event");
    }

    void MapSharedMem(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x01, 1, 2};
        const u32 size{rp.Pop<u32>()};
        shared_memory = rp.PopObject<Kernel::SharedMemory>();
        if (shared_memory)
            shared_memory->SetName("mic:u Shared Memory");
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_MIC, "size=0x{:X}", size);
    }

    void UnmapSharedMem(Kernel::HLERequestContext& ctx) {
        shared_memory = nullptr;
        IPC::ResponseBuilder rb{ctx, 0x02, 1, 0};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_MIC, "called");
    }

    void StartSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x03, 5, 0};
        encoding = rp.PopEnum<Encoding>();
        sample_rate = rp.PopEnum<SampleRate>();
        audio_buffer_offset = rp.Pop<u32>();
        audio_buffer_size = rp.Pop<u32>();
        audio_buffer_loop = rp.Pop<bool>();
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(RESULT_SUCCESS);
        UNIMPLEMENTED();
        LOG_DEBUG(Service_MIC,
                  "encoding={}, sample_rate={}, "
                  "audio_buffer_offset={}, audio_buffer_size={}, audio_buffer_loop={}",
                  static_cast<u32>(encoding), static_cast<u32>(sample_rate), audio_buffer_offset,
                  audio_buffer_size, audio_buffer_loop);
    }

    void AdjustSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x04, 1, 0};
        sample_rate = rp.PopEnum<SampleRate>();
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(RESULT_SUCCESS);
        UNIMPLEMENTED();
        LOG_DEBUG(Service_MIC, "sample_rate={}", static_cast<u32>(sample_rate));
    }

    void StopSampling(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 0x05, 1, 0};
        rb.Push(RESULT_SUCCESS);
        UNIMPLEMENTED();
    }

    void IsSampling(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 0x06, 2, 0};
        rb.Push(RESULT_SUCCESS);
        rb.Push<bool>(is_sampling);
        LOG_DEBUG(Service_MIC, "is_sampling={}", is_sampling);
    }

    void GetBufferFullEvent(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 0x07, 1, 2};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(buffer_full_event);
        LOG_DEBUG(Service_MIC, "called");
    }

    void SetGain(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x08, 1, 0};
        mic_gain = rp.Pop<u8>();
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_MIC, "mic_gain={}", mic_gain);
    }

    void GetGain(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 0x09, 2, 0};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u8>(mic_gain);
        LOG_DEBUG(Service_MIC, "mic_gain={}", mic_gain);
    }

    void SetPower(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0A, 1, 0};
        mic_power = rp.Pop<bool>();
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_MIC, "mic_power={}", mic_power);
    }

    void GetPower(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 0x0B, 2, 0};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u8>(mic_power);
        LOG_DEBUG(Service_MIC, "mic_power={}", mic_power);
    }

    void SetIirFilterMic(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0C, 1, 2};
        const u32 size{rp.Pop<u32>()};
        const Kernel::MappedBuffer& buffer{rp.PopMappedBuffer()};
        auto rb{rp.MakeBuilder(1, 2)};
        rb.Push(RESULT_SUCCESS);
        rb.PushMappedBuffer(buffer);
        LOG_DEBUG(Service_MIC, "size=0x{:X}, buffer=0x{:08X}", size, buffer.GetID());
    }

    void SetClamp(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0D, 1, 0};
        clamp = rp.Pop<bool>();
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_MIC, "clamp={}", clamp);
    }

    void GetClamp(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 0x0E, 2, 0};
        rb.Push(RESULT_SUCCESS);
        rb.Push<bool>(clamp);
        LOG_DEBUG(Service_MIC, "clamp={}", clamp);
    }

    void SetAllowShellClosed(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x0F, 1, 0};
        allow_shell_closed = rp.Pop<bool>();
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_MIC, "allow_shell_closed={}", allow_shell_closed);
    }

    void SetClientVersion(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx, 0x10, 1, 0};
        const u32 version{rp.Pop<u32>()};
        client_version = version;
        auto rb{rp.MakeBuilder(1, 0)};
        rb.Push(RESULT_SUCCESS);
        LOG_DEBUG(Service_MIC, "version=0x{:08X}", version);
    }

    u32 client_version{};
    Kernel::SharedPtr<Kernel::Event> buffer_full_event;
    Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;
    u8 mic_gain{};
    bool mic_power{};
    bool is_sampling{};
    bool allow_shell_closed{};
    bool clamp{};
    Encoding encoding{Encoding::PCM8};
    SampleRate sample_rate{SampleRate::SampleRate32730};
    u32 audio_buffer_offset{};
    u32 audio_buffer_size{};
    bool audio_buffer_loop{};
};

void MIC_U::MapSharedMem(Kernel::HLERequestContext& ctx) {
    impl->MapSharedMem(ctx);
}

void MIC_U::UnmapSharedMem(Kernel::HLERequestContext& ctx) {
    impl->UnmapSharedMem(ctx);
}

void MIC_U::StartSampling(Kernel::HLERequestContext& ctx) {
    impl->StartSampling(ctx);
}

void MIC_U::AdjustSampling(Kernel::HLERequestContext& ctx) {
    impl->AdjustSampling(ctx);
}

void MIC_U::StopSampling(Kernel::HLERequestContext& ctx) {
    impl->StopSampling(ctx);
}

void MIC_U::IsSampling(Kernel::HLERequestContext& ctx) {
    impl->IsSampling(ctx);
}

void MIC_U::GetBufferFullEvent(Kernel::HLERequestContext& ctx) {
    impl->GetBufferFullEvent(ctx);
}

void MIC_U::SetGain(Kernel::HLERequestContext& ctx) {
    impl->SetGain(ctx);
}

void MIC_U::GetGain(Kernel::HLERequestContext& ctx) {
    impl->GetGain(ctx);
}

void MIC_U::SetPower(Kernel::HLERequestContext& ctx) {
    impl->SetPower(ctx);
}

void MIC_U::GetPower(Kernel::HLERequestContext& ctx) {
    impl->GetPower(ctx);
}

void MIC_U::SetIirFilterMic(Kernel::HLERequestContext& ctx) {
    impl->SetIirFilterMic(ctx);
}

void MIC_U::SetClamp(Kernel::HLERequestContext& ctx) {
    impl->SetClamp(ctx);
}

void MIC_U::GetClamp(Kernel::HLERequestContext& ctx) {
    impl->GetClamp(ctx);
}

void MIC_U::SetAllowShellClosed(Kernel::HLERequestContext& ctx) {
    impl->SetAllowShellClosed(ctx);
}

void MIC_U::SetClientVersion(Kernel::HLERequestContext& ctx) {
    impl->SetClientVersion(ctx);
}

MIC_U::MIC_U(Core::System& system)
    : ServiceFramework{"mic:u", 1}, impl{std::make_unique<Impl>(system)} {
    static const FunctionInfo functions[]{
        {0x00010042, &MIC_U::MapSharedMem, "MapSharedMem"},
        {0x00020000, &MIC_U::UnmapSharedMem, "UnmapSharedMem"},
        {0x00030140, &MIC_U::StartSampling, "StartSampling"},
        {0x00040040, &MIC_U::AdjustSampling, "AdjustSampling"},
        {0x00050000, &MIC_U::StopSampling, "StopSampling"},
        {0x00060000, &MIC_U::IsSampling, "IsSampling"},
        {0x00070000, &MIC_U::GetBufferFullEvent, "GetBufferFullEvent"},
        {0x00080040, &MIC_U::SetGain, "SetGain"},
        {0x00090000, &MIC_U::GetGain, "GetGain"},
        {0x000A0040, &MIC_U::SetPower, "SetPower"},
        {0x000B0000, &MIC_U::GetPower, "GetPower"},
        {0x000C0042, &MIC_U::SetIirFilterMic, "SetIirFilterMic"},
        {0x000D0040, &MIC_U::SetClamp, "SetClamp"},
        {0x000E0000, &MIC_U::GetClamp, "GetClamp"},
        {0x000F0040, &MIC_U::SetAllowShellClosed, "SetAllowShellClosed"},
        {0x00100040, &MIC_U::SetClientVersion, "SetClientVersion"},
    };
    RegisterHandlers(functions);
}

MIC_U::~MIC_U() {}

void InstallInterfaces(Core::System& system) {
    std::make_shared<MIC_U>(system)->InstallAsService(system.ServiceManager());
}

} // namespace Service::MIC
