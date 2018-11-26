// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <cstring>
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend.h"
#include "core/hle/kernel/shared_page.h"
#include "core/movie.h"
#include "core/settings.h"

namespace SharedPage {

static std::chrono::seconds GetInitTime(Core::Movie& movie) {
    u64 override_init_time{movie.GetOverrideInitTime()};
    if (override_init_time)
        // Override the clock init time with the one in the movie
        return std::chrono::seconds{override_init_time};
    switch (Settings::values.init_clock) {
    case Settings::InitClock::SystemTime: {
        auto now{std::chrono::system_clock::now()};
        // If the system time is in daylight saving, we give an additional hour to console time
        auto now_time_t{std::chrono::system_clock::to_time_t(now)};
        auto now_tm{std::localtime(&now_time_t)};
        if (now_tm && now_tm->tm_isdst > 0)
            now = now + std::chrono::hours(1);
        return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    }
    case Settings::InitClock::FixedTime:
        return std::chrono::seconds{Settings::values.init_time};
    default:
        UNREACHABLE();
    }
}

Handler::Handler(Core::System& system)
    : timing{system.CoreTiming()}, frontend{system.GetFrontend()} {
    std::memset(&shared_page, 0, sizeof(shared_page));
    shared_page.running_hw = 0x1; // Product
    // Some games wait until this value becomes 0x1, before asking running_hw
    shared_page.unknown_value = 0x1;
    shared_page.battery_state.charge_level.Assign(
        static_cast<u8>(Settings::values.p_battery_level));
    shared_page.battery_state.is_adapter_connected.Assign(
        static_cast<u8>(Settings::values.p_adapter_connected));
    shared_page.battery_state.is_charging.Assign(
        static_cast<u8>(Settings::values.p_battery_charging));
    shared_page.wifi_link_level = Settings::values.n_wifi_link_level;
    shared_page.network_state = static_cast<NetworkState>(Settings::values.n_state);
    Update3DSettings();
    init_time = GetInitTime(system.MovieSystem());
    using namespace std::placeholders;
    update_time_event = timing.RegisterEvent("Shared Page Time Update Event",
                                             std::bind(&Handler::UpdateTimeCallback, this, _1, _2));
    timing.ScheduleEvent(0, update_time_event);
}

/// Gets system time in real console format. The epoch is Jan 1900, and the unit is millisecond.
u64 Handler::GetSystemTime() const {
    std::chrono::milliseconds now{init_time + std::chrono::duration_cast<std::chrono::milliseconds>(
                                                  timing.GetGlobalTimeUs())};
    // Real console does't allow user to set a time before Jan 1 2000,
    // so we use it as an auxiliary epoch to calculate the console time.
    std::tm epoch_tm{};
    epoch_tm.tm_sec = 0;
    epoch_tm.tm_min = 0;
    epoch_tm.tm_hour = 0;
    epoch_tm.tm_mday = 1;
    epoch_tm.tm_mon = 0;
    epoch_tm.tm_year = 100;
    epoch_tm.tm_isdst = 0;
    s64 epoch{std::mktime(&epoch_tm) * 1000};
    // Real console time uses Jan 1 1900 as internal epoch,
    // so we use the milliseconds between 1900 and 2000 as base console time
    u64 console_time{3155673600000ULL};
    // Only when system time is after 2000, we set it as console system time
    if (now.count() > epoch)
        console_time += (now.count() - epoch);
    return console_time;
}

void Handler::UpdateTimeCallback(u64 userdata, int cycles_late) {
    auto& date_time{shared_page.date_time_counter % 2 ? shared_page.date_time_0
                                                      : shared_page.date_time_1};
    date_time.date_time = GetSystemTime();
    date_time.update_tick = timing.GetTicks();
    date_time.tick_to_second_coefficient = BASE_CLOCK_RATE_ARM11;
    date_time.tick_offset = 0;
    ++shared_page.date_time_counter;
    // System time is updated hourly
    timing.ScheduleEvent(msToCycles(60 * 60 * 1000) - cycles_late, update_time_event);
}

void Handler::SetMACAddress(const MACAddress& addr) {
    std::memcpy(shared_page.wifi_macaddr, addr.data(), sizeof(MACAddress));
}

void Handler::SetWiFiLinkLevel(WiFiLinkLevel level) {
    shared_page.wifi_link_level = static_cast<u8>(level);
}

void Handler::SetNetworkState(NetworkState state) {
    shared_page.network_state = state;
}

NetworkState Handler::GetNetworkState() {
    return shared_page.network_state;
}

void Handler::SetAdapterConnected(u8 adapter_connected) {
    shared_page.battery_state.is_adapter_connected.Assign(adapter_connected);
}

void Handler::SetBatteryCharging(u8 charging) {
    shared_page.battery_state.is_charging.Assign(charging);
}

void Handler::SetBatteryLevel(u8 level) {
    shared_page.battery_state.charge_level.Assign(level);
}

SharedPageDef& Handler::GetSharedPage() {
    return shared_page;
}

void Handler::Update3DSettings(bool called_by_control_panel) {
    if (Settings::values.disable_mh_2xmsaa) {
        shared_page.sliderstate_3d = 0.01f;
        shared_page.ledstate_3d = 0;
        return;
    }
    shared_page.ledstate_3d = Settings::values.factor_3d == 0 ? 1 : 0;
    shared_page.sliderstate_3d =
        Settings::values.factor_3d != 0 ? (float_le)Settings::values.factor_3d / 100 : 0.0f;
    if (!called_by_control_panel)
        frontend.Update3D();
}

} // namespace SharedPage
