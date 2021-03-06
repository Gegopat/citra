// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <unordered_set>
#include "audio_core/dsp_interface.h"
#include "audio_core/sink.h"
#include "common/assert.h"
#include "core/core.h"
#include "core/settings.h"

namespace AudioCore {

static const std::unordered_set<u64> ids_output_allowed_shell_closed{{
    // Nintendo 3DS Sound
    0x0004001000020500,
    0x0004001000021500,
    0x0004001000022500,
    0x0004001000026500,
    0x0004001000027500,
    0x0004001000028500,
}};

DspInterface::DspInterface(Core::System& system) : system{system} {
    UpdateSink();
}

DspInterface::~DspInterface() = default;

void DspInterface::UpdateSink() {
    sink = std::make_unique<Sink>(Settings::values.output_device);
    sink->SetCallback(
        [this](s16* buffer, std::size_t num_frames) { OutputCallback(buffer, num_frames); });
}

void DspInterface::EnableStretching(bool enable) {
    if (perform_time_stretching == enable)
        return;
    if (!enable)
        flushing_time_stretcher = true;
    perform_time_stretching = enable;
}

void DspInterface::OutputFrame(StereoFrame16& frame) {
    if (!IsOutputAllowed())
        return;
    fifo.Push(frame.data(), frame.size());
}

void DspInterface::OutputSample(std::array<s16, 2> sample) {
    if (!IsOutputAllowed())
        return;
    fifo.Push(&sample, 1);
}

void DspInterface::OutputCallback(s16* buffer, std::size_t num_frames) {
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

bool DspInterface::IsOutputAllowed() {
    if (!sink)
        return false;
    else if (!system.IsSleepModeEnabled())
        return true;
    else
        return ids_output_allowed_shell_closed.count(
                   system.Kernel().GetCurrentProcess()->codeset->program_id) == 1 &&
               Settings::values.headphones_connected;
}

} // namespace AudioCore
