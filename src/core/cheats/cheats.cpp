// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <functional>
#include <fmt/format.h>
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/process.h"
#include "core/settings.h"

namespace Cheats {

inline u64 GetRunIntervalTicks() {
    return BASE_CLOCK_RATE_ARM11 / Settings::values.screen_refresh_rate;
}

CheatEngine::CheatEngine(Core::System& system) : system{system} {
    LoadCheatFile();
    event = system.CoreTiming().RegisterEvent(
        "Cheats Run Event",
        [this](u64 thread_id, s64 cycle_late) { RunCallback(thread_id, cycle_late); });
    system.CoreTiming().ScheduleEvent(GetRunIntervalTicks(), event);
}

CheatEngine::~CheatEngine() {
    system.CoreTiming().UnscheduleEvent(event, 0);
}

const std::vector<std::unique_ptr<CheatBase>>& CheatEngine::GetCheats() const {
    return cheats_list;
}

void CheatEngine::LoadCheatFile() {
    const auto cheat_dir{FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir)};
    const auto filepath{fmt::format("{}{:016X}.txt", cheat_dir,
                                    system.Kernel().GetCurrentProcess()->codeset->program_id)};
    if (!FileUtil::IsDirectory(cheat_dir))
        FileUtil::CreateDir(cheat_dir);
    if (!FileUtil::Exists(filepath))
        return;
    auto gateway_cheats{GatewayCheat::LoadFile(filepath)};
    std::move(gateway_cheats.begin(), gateway_cheats.end(), std::back_inserter(cheats_list));
}

void CheatEngine::RunCallback(u64, int cycles_late) {
    for (auto& cheat : cheats_list)
        if (cheat->IsEnabled())
            cheat->Execute(system);
    system.CoreTiming().ScheduleEvent(GetRunIntervalTicks() - cycles_late, event);
}

} // namespace Cheats
