// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <ctime>
#include <memory>
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/memory.h"

namespace Core {
class System;
struct TimingEventType;
} // namespace Core

namespace SharedPage {

/**
 * The shared page stores various runtime configuration settings. This memory page is
 * read-only for user processes (there is a bit in the header that grants the process
 * write access, according to 3dbrew; this isn't emulated)
 */

// See http://3dbrew.org/wiki/Configuration_Memory#Shared_Memory_Page_For_ARM11_Processes

struct DateTime {
    u64_le date_time;                  // 0
    u64_le update_tick;                // 8
    u64_le tick_to_second_coefficient; // 10
    u64_le tick_offset;                // 18
};
static_assert(sizeof(DateTime) == 0x20, "Datetime size is wrong");

union BatteryState {
    u8 raw;
    BitField<0, 1, u8> is_adapter_connected;
    BitField<1, 1, u8> is_charging;
    BitField<2, 3, u8> charge_level;
};

// Default MAC address in valid range
constexpr MacAddress DefaultMac{0x40, 0xF4, 0x07, 0x00, 0x00, 0x00};

enum class WifiLinkLevel : u8 {
    Off = 0,
    Poor = 1,
    Good = 2,
    Best = 3,
};

enum class NetworkState : u8 {
    Enabled = 0,
    Internet = 2,
    Local = 3,
    Disabled = 7,
};

struct SharedPageDef {
    // Most of these names are taken from the 3dbrew page linked above.
    u32_le date_time_counter; // 0
    u8 running_hw;            // 4
    /// "Microcontroller hardware info"
    u8 mcu_hw_info;                      // 5
    INSERT_PADDING_BYTES(0x20 - 0x6);    // 6
    DateTime date_time_0;                // 20
    DateTime date_time_1;                // 40
    u8 wifi_macaddr[6];                  // 60
    u8 wifi_link_level;                  // 66
    NetworkState network_state;          // 67
    INSERT_PADDING_BYTES(0x80 - 0x68);   // 68
    float_le sliderstate_3d;             // 80
    u8 ledstate_3d;                      // 84
    BatteryState battery_state;          // 85
    u8 unknown_value;                    // 86
    INSERT_PADDING_BYTES(0xA0 - 0x87);   // 87
    u64_le menu_program_id_;             // A0
    u64_le active_menu_program_id_;      // A8
    INSERT_PADDING_BYTES(0x1000 - 0xB0); // B0
};
static_assert(sizeof(SharedPageDef) == Memory::SHARED_PAGE_SIZE,
              "Shared page structure size is wrong");

class Handler {
public:
    explicit Handler(Core::System& system);

    void SetMacAddress(const MacAddress&);
    void SetWifiLinkLevel(WifiLinkLevel);
    void SetNetworkState(NetworkState);
    NetworkState GetNetworkState();
    void SetAdapterConnected(u8);
    void SetBatteryCharging(u8);
    void SetBatteryLevel(u8);
    SharedPageDef& GetSharedPage();
    void Update3DSettings(bool called_by_control_panel = false);

private:
    u64 GetSystemTime() const;
    void UpdateTimeCallback(u64 userdata, int cycles_late);

    Core::TimingEventType* update_time_event;
    std::chrono::seconds init_time;
    SharedPageDef shared_page;
    Core::Timing& timing;
    Frontend& frontend;
};

} // namespace SharedPage
