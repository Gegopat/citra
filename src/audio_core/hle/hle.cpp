// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_set>
#include "audio_core/audio_types.h"
#include "audio_core/hle/common.h"
#include "audio_core/hle/hle.h"
#include "audio_core/hle/mixers.h"
#include "audio_core/hle/shared_memory.h"
#include "audio_core/hle/source.h"
#include "audio_core/sink.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/dsp/dsp_dsp.h"
#include "core/settings.h"

using InterruptType = Service::DSP::DSP_DSP::InterruptType;
using Service::DSP::DSP_DSP;

namespace AudioCore {

constexpr u64 audio_frame_ticks{1310252ull}; ///< Units: ARM11 cycles

static const std::unordered_set<u64> ids_output_allowed_shell_closed{{
    // Nintendo 3DS Sound
    0x0004001000020500,
    0x0004001000021500,
    0x0004001000022500,
    0x0004001000026500,
    0x0004001000027500,
    0x0004001000028500,
}};

struct DspHle::Impl final {
public:
    explicit Impl(DspHle& parent, Core::System& system);
    ~Impl();

    DspState GetDspState() const;

    std::vector<u8> PipeRead(DspPipe pipe_number, u32 length);
    std::size_t GetPipeReadableSize(DspPipe pipe_number) const;
    void PipeWrite(DspPipe pipe_number, const std::vector<u8>& buffer);

    std::array<u8, Memory::DSP_RAM_SIZE>& GetDspMemory();

    void SetServiceToInterrupt(std::weak_ptr<DSP_DSP> dsp);

    bool IsOutputAllowed();

private:
    void ResetPipes();
    void WriteU16(DspPipe pipe_number, u16 value);
    void AudioPipeWriteStructAddresses();

    std::size_t CurrentRegionIndex() const;
    HLE::SharedMemory& ReadRegion();
    HLE::SharedMemory& WriteRegion();

    StereoFrame16 GenerateCurrentFrame();
    bool Tick();
    void AudioTickCallback(s64 cycles_late);

    DspState dsp_state{DspState::Off};
    std::array<std::vector<u8>, num_dsp_pipe> pipe_data;
    HLE::DspMemory dsp_memory;

    std::array<HLE::Source, HLE::num_sources> sources{{
        HLE::Source{0},  HLE::Source{1},  HLE::Source{2},  HLE::Source{3},  HLE::Source{4},
        HLE::Source{5},  HLE::Source{6},  HLE::Source{7},  HLE::Source{8},  HLE::Source{9},
        HLE::Source{10}, HLE::Source{11}, HLE::Source{12}, HLE::Source{13}, HLE::Source{14},
        HLE::Source{15}, HLE::Source{16}, HLE::Source{17}, HLE::Source{18}, HLE::Source{19},
        HLE::Source{20}, HLE::Source{21}, HLE::Source{22}, HLE::Source{23},
    }};

    HLE::Mixers mixers;

    DspHle& parent;
    Core::TimingEventType* tick_event;

    std::weak_ptr<DSP_DSP> dsp_dsp;

    Core::System& system;
};

DspHle::Impl::Impl(DspHle& parent_, Core::System& system_) : parent{parent_}, system{system_} {
    auto& memory{system.Memory()};
    for (auto& s : sources)
        s.SetMemory(memory);
    dsp_memory.raw_memory.fill(0);
    Core::Timing& timing{system.CoreTiming()};
    tick_event = timing.RegisterEvent(
        "DSP Tick Event", [this](u64, s64 cycles_late) { AudioTickCallback(cycles_late); });
    timing.ScheduleEvent(audio_frame_ticks, tick_event);
}

DspHle::Impl::~Impl() {
    system.CoreTiming().UnscheduleEvent(tick_event, 0);
}

DspState DspHle::Impl::GetDspState() const {
    return dsp_state;
}

std::vector<u8> DspHle::Impl::PipeRead(DspPipe pipe_number, u32 length) {
    const std::size_t pipe_index{static_cast<std::size_t>(pipe_number)};
    if (pipe_index >= num_dsp_pipe) {
        LOG_ERROR(Audio_DSP, "pipe_number {} invalid", pipe_index);
        return {};
    }
    if (length > UINT16_MAX) { // Can only read at most UINT16_MAX from the pipe
        LOG_ERROR(Audio_DSP, "length of {} greater than max of {}", length, UINT16_MAX);
        return {};
    }
    std::vector<u8>& data{pipe_data[pipe_index]};
    if (length > data.size()) {
        LOG_WARNING(Audio_DSP, "pipe {} is out of data, program requested read of {} but {} remain",
                    pipe_index, length, data.size());
        length = static_cast<u32>(data.size());
    }
    if (length == 0)
        return {};
    std::vector<u8> ret{data.begin(), data.begin() + length};
    data.erase(data.begin(), data.begin() + length);
    return ret;
}

std::size_t DspHle::Impl::GetPipeReadableSize(DspPipe pipe_number) const {
    const std::size_t pipe_index{static_cast<std::size_t>(pipe_number)};
    if (pipe_index >= num_dsp_pipe) {
        LOG_ERROR(Audio_DSP, "pipe_number {} invalid", pipe_index);
        return 0;
    }
    return pipe_data[pipe_index].size();
}

void DspHle::Impl::PipeWrite(DspPipe pipe_number, const std::vector<u8>& buffer) {
    switch (pipe_number) {
    case DspPipe::Audio: {
        if (buffer.size() != 4) {
            LOG_ERROR(Audio_DSP, "DspPipe::Audio: Unexpected buffer length {} was written",
                      buffer.size());
            return;
        }
        enum class StateChange {
            Initialize = 0,
            Shutdown = 1,
            Wakeup = 2,
            Sleep = 3,
        };
        // The difference between Initialize and Wakeup is that Input state is maintained
        // when sleeping but isn't when turning it off and on again. (TODO: Implement this.)
        // Waking up from sleep garbles some of the structs in the memory region. (TODO:
        // Implement this.) Programs store away the state of these structs before
        // sleeping and reset it back after wakeup on behalf of the DSP.
        switch (static_cast<StateChange>(buffer[0])) {
        case StateChange::Initialize:
            LOG_INFO(Audio_DSP, "Program has requested initialization of DSP hardware");
            ResetPipes();
            AudioPipeWriteStructAddresses();
            dsp_state = DspState::On;
            break;
        case StateChange::Shutdown:
            LOG_INFO(Audio_DSP, "Program has requested shutdown of DSP hardware");
            dsp_state = DspState::Off;
            break;
        case StateChange::Wakeup:
            LOG_INFO(Audio_DSP, "Program has requested wakeup of DSP hardware");
            ResetPipes();
            AudioPipeWriteStructAddresses();
            dsp_state = DspState::On;
            break;
        case StateChange::Sleep:
            LOG_INFO(Audio_DSP, "Program has requested sleep of DSP hardware");
            UNIMPLEMENTED();
            dsp_state = DspState::Sleeping;
            break;
        default:
            LOG_ERROR(Audio_DSP,
                      "Program has requested unknown state transition of DSP hardware {}",
                      buffer[0]);
            dsp_state = DspState::Off;
            break;
        }
        std::copy(buffer.begin(), buffer.end(),
                  std::back_inserter(pipe_data[static_cast<std::size_t>(DspPipe::Binary)]));
        return;
    }
    case DspPipe::Binary:
        std::copy(buffer.begin(), buffer.end(),
                  std::back_inserter(pipe_data[static_cast<std::size_t>(DspPipe::Binary)]));
        return;
    default:
        UNIMPLEMENTED_MSG("pipe_number={} unimplemented", static_cast<std::size_t>(pipe_number));
        return;
    }
}

std::array<u8, Memory::DSP_RAM_SIZE>& DspHle::Impl::GetDspMemory() {
    return dsp_memory.raw_memory;
}

void DspHle::Impl::SetServiceToInterrupt(std::weak_ptr<DSP_DSP> dsp) {
    dsp_dsp = std::move(dsp);
}

void DspHle::Impl::ResetPipes() {
    for (auto& data : pipe_data)
        data.clear();
    dsp_state = DspState::Off;
}

void DspHle::Impl::WriteU16(DspPipe pipe_number, u16 value) {
    const std::size_t pipe_index{static_cast<std::size_t>(pipe_number)};
    std::vector<u8>& data{pipe_data.at(pipe_index)};
    // Little endian
    data.emplace_back(value & 0xFF);
    data.emplace_back(value >> 8);
}

void DspHle::Impl::AudioPipeWriteStructAddresses() {
    // These struct addresses are DSP dram addresses.
    // See also: DSP_DSP::ConvertProcessAddressFromDspDram
    static const std::array<u16, 15> struct_addresses{{
        0x8000 + offsetof(HLE::SharedMemory, frame_counter) / 2,
        0x8000 + offsetof(HLE::SharedMemory, source_configurations) / 2,
        0x8000 + offsetof(HLE::SharedMemory, source_statuses) / 2,
        0x8000 + offsetof(HLE::SharedMemory, adpcm_coefficients) / 2,
        0x8000 + offsetof(HLE::SharedMemory, dsp_configuration) / 2,
        0x8000 + offsetof(HLE::SharedMemory, dsp_status) / 2,
        0x8000 + offsetof(HLE::SharedMemory, final_samples) / 2,
        0x8000 + offsetof(HLE::SharedMemory, intermediate_mix_samples) / 2,
        0x8000 + offsetof(HLE::SharedMemory, compressor) / 2,
        0x8000 + offsetof(HLE::SharedMemory, dsp_debug) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown10) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown11) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown12) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown13) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown14) / 2,
    }};
    // Begin with a u16 denoting the number of structs.
    WriteU16(DspPipe::Audio, static_cast<u16>(struct_addresses.size()));
    // Then write the struct addresses.
    for (u16 addr : struct_addresses)
        WriteU16(DspPipe::Audio, addr);
    // Signal that we have data on this pipe.
    if (auto service{dsp_dsp.lock()})
        service->SignalInterrupt(InterruptType::Pipe, DspPipe::Audio);
}

std::size_t DspHle::Impl::CurrentRegionIndex() const {
    // The region with the higher frame counter is chosen unless there is wraparound.
    // This function only returns a 0 or 1.
    const u16 frame_counter_0{dsp_memory.region_0.frame_counter};
    const u16 frame_counter_1{dsp_memory.region_1.frame_counter};
    if (frame_counter_0 == 0xFFFFu && frame_counter_1 != 0xFFFEu)
        // Wraparound has occurred.
        return 1;
    if (frame_counter_1 == 0xFFFFu && frame_counter_0 != 0xFFFEu)
        // Wraparound has occurred.
        return 0;
    return (frame_counter_0 > frame_counter_1) ? 0 : 1;
}

HLE::SharedMemory& DspHle::Impl::ReadRegion() {
    return CurrentRegionIndex() == 0 ? dsp_memory.region_0 : dsp_memory.region_1;
}

HLE::SharedMemory& DspHle::Impl::WriteRegion() {
    return CurrentRegionIndex() != 0 ? dsp_memory.region_0 : dsp_memory.region_1;
}

StereoFrame16 DspHle::Impl::GenerateCurrentFrame() {
    HLE::SharedMemory& read{ReadRegion()};
    HLE::SharedMemory& write{WriteRegion()};
    std::array<QuadFrame32, 3> intermediate_mixes{};
    // Generate intermediate mixes
    for (std::size_t i{}; i < HLE::num_sources; i++) {
        write.source_statuses.status[i] =
            sources[i].Tick(read.source_configurations.config[i], read.adpcm_coefficients.coeff[i]);
        for (std::size_t mix{}; mix < 3; mix++)
            sources[i].MixInto(intermediate_mixes[mix], mix);
    }
    // Generate final mix
    write.dsp_status = mixers.Tick(read.dsp_configuration, read.intermediate_mix_samples,
                                   write.intermediate_mix_samples, intermediate_mixes);
    StereoFrame16 output_frame{mixers.GetOutput()};
    // Write current output frame to the shared memory region
    for (std::size_t samplei{}; samplei < output_frame.size(); samplei++)
        for (std::size_t channeli{}; channeli < output_frame[0].size(); channeli++)
            write.final_samples.pcm16[samplei][channeli] = s16_le(output_frame[samplei][channeli]);
    return output_frame;
}

bool DspHle::Impl::Tick() {
    if (!IsOutputAllowed())
        return false;
    // TODO: Check dsp::DSP semaphore (which indicates emulated program has finished writing to
    // shared memory region)
    parent.OutputFrame(GenerateCurrentFrame());
    return true;
}

bool DspHle::Impl::IsOutputAllowed() {
    if (!system.IsSleepModeEnabled())
        return true;
    else
        return ids_output_allowed_shell_closed.count(
                   system.Kernel().GetCurrentProcess()->codeset->program_id) == 1 &&
               Settings::values.headphones_connected;
}

void DspHle::Impl::AudioTickCallback(s64 cycles_late) {
    if (Tick())
        // TODO: Signal all the other interrupts as appropriate.
        if (auto service{dsp_dsp.lock()}) {
            service->SignalInterrupt(InterruptType::Pipe, DspPipe::Audio);
            // HACK: Added to prevent regressions. Will remove soon.
            service->SignalInterrupt(InterruptType::Pipe, DspPipe::Binary);
        }
    // Reschedule recurrent event
    Core::Timing& timing{system.CoreTiming()};
    timing.ScheduleEvent(audio_frame_ticks - cycles_late, tick_event);
}

DspHle::DspHle(Core::System& system)
    : impl{std::make_unique<Impl>(*this, system)}, sink{std::make_unique<Sink>(
                                                       Settings::values.output_device)} {
    sink->SetCallback(
        [this](s16* buffer, std::size_t num_frames) { OutputCallback(buffer, num_frames); });
}

DspHle::~DspHle() = default;

DspState DspHle::GetDspState() const {
    return impl->GetDspState();
}

std::vector<u8> DspHle::PipeRead(DspPipe pipe_number, u32 length) {
    return impl->PipeRead(pipe_number, length);
}

std::size_t DspHle::GetPipeReadableSize(DspPipe pipe_number) const {
    return impl->GetPipeReadableSize(pipe_number);
}

void DspHle::PipeWrite(DspPipe pipe_number, const std::vector<u8>& buffer) {
    impl->PipeWrite(pipe_number, buffer);
}

std::array<u8, Memory::DSP_RAM_SIZE>& DspHle::GetDspMemory() {
    return impl->GetDspMemory();
}

void DspHle::SetServiceToInterrupt(std::weak_ptr<DSP_DSP> dsp) {
    impl->SetServiceToInterrupt(std::move(dsp));
}

void DspHle::UpdateSink() {
    sink = std::make_unique<Sink>(Settings::values.output_device);
    sink->SetCallback(
        [this](s16* buffer, std::size_t num_frames) { OutputCallback(buffer, num_frames); });
}

void DspHle::EnableStretching(bool enable) {
    if (perform_time_stretching == enable)
        return;
    if (!enable)
        flushing_time_stretcher = true;
    perform_time_stretching = enable;
}

void DspHle::OutputFrame(const StereoFrame16& frame) {
    if (!sink)
        return;
    fifo.Push(frame.data(), frame.size());
}

void DspHle::OutputCallback(s16* buffer, std::size_t num_frames) {
    std::size_t frames_written{};
    if (perform_time_stretching) {
        const std::vector<s16> in{fifo.Pop()};
        const std::size_t num_in{in.size() / 2};
        frames_written = time_stretcher.Process(in.data(), num_in, buffer, num_frames);
    } else if (flushing_time_stretcher) {
        time_stretcher.Flush();
        frames_written = time_stretcher.Process(nullptr, 0, buffer, num_frames);
        frames_written += fifo.Pop(buffer, num_frames - frames_written);
        flushing_time_stretcher = false;
    } else
        frames_written = fifo.Pop(buffer, num_frames);
    if (frames_written > 0)
        std::memcpy(&last_frame[0], buffer + 2 * (frames_written - 1), 2 * sizeof(s16));
    // Hold last emitted frame; this prevents popping.
    for (std::size_t i{frames_written}; i < num_frames; i++)
        std::memcpy(buffer + 2 * i, &last_frame[0], 2 * sizeof(s16));
    // Implementation of the hardware volume slider with a dynamic range of 60 dB
    const float linear_volume{std::clamp(Settings::values.volume, 0.0f, 1.0f)};
    if (linear_volume != 1.0) {
        const float volume_scale_factor{std::exp(6.90775f * linear_volume) * 0.001f};
        for (std::size_t i{}; i < num_frames; i++) {
            buffer[i * 2 + 0] = static_cast<s16>(buffer[i * 2 + 0] * volume_scale_factor);
            buffer[i * 2 + 1] = static_cast<s16>(buffer[i * 2 + 1] * volume_scale_factor);
        }
    }
}

bool DspHle::IsOutputAllowed() {
    return impl->IsOutputAllowed();
}

} // namespace AudioCore
