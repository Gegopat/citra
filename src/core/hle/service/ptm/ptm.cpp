// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/ptm/ptm_gets.h"
#include "core/hle/service/ptm/ptm_play.h"
#include "core/hle/service/ptm/ptm_sets.h"
#include "core/hle/service/ptm/ptm_sysm.h"
#include "core/hle/service/ptm/ptm_u.h"
#include "core/settings.h"

namespace Service::PTM {

/// Values for the default gamecoin.dat file
constexpr GameCoin default_game_coin{0x4F00, 42, 0, 0, 0, 2014, 12, 29};

void Module::Interface::GetAdapterState(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x5, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push(Settings::values.p_adapter_connected);
}

void Module::Interface::GetShellState(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x6, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push(!ptm->system.IsSleepModeEnabled());
}

void Module::Interface::GetBatteryLevel(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x7, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push(Settings::values.p_battery_level);
}

void Module::Interface::GetBatteryChargeState(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x8, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push(Settings::values.p_battery_charging);
}

void Module::Interface::GetPedometerState(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x9, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push(ptm->pedometer_is_counting);
    LOG_WARNING(Service_PTM, "stubbed");
}

void Module::Interface::GetStepHistory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0xB, 3, 2};
    u32 hours{rp.Pop<u32>()};
    u64 start_time{rp.Pop<u64>()};
    auto& buffer{rp.PopMappedBuffer()};
    ASSERT_MSG(sizeof(u16) * hours == buffer.GetSize(),
               "Buffer for steps count has incorrect size");
    // Stub: set zero steps count for every hour
    for (u32 i{}; i < hours; ++i) {
        const u16_le steps_per_hour{};
        buffer.Write(&steps_per_hour, i * sizeof(u16), sizeof(u16));
    }
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);
    LOG_WARNING(Service_PTM, "(stubbed) from time(raw): 0x{:X}, for {} hours", start_time, hours);
}

void Module::Interface::GetTotalStepCount(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0xC, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
    LOG_WARNING(Service_PTM, "stubbed");
}

void Module::Interface::GetSoftwareClosedFlag(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x80F, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push(false);
    LOG_WARNING(Service_PTM, "stubbed");
}

void Module::Interface::ConfigureNew3DSCPU(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x818, 1, 0};
    u8 value{static_cast<u8>(rp.Pop<u8>() & 0xF)};
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_PTM, "stubbed");
}

void Module::Interface::CheckNew3DS(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x40A, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push(ptm->system.ServiceManager()
                .GetService<CFG::Module::Interface>("cfg:u")
                ->GetModule()
                ->GetNewModel());
}

Module::Module(Core::System& system) : system{system} {
    auto nand_directory{
        FileUtil::GetUserPath(FileUtil::UserPath::NANDDir, Settings::values.nand_dir)};
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory{nand_directory, true};
    // Open the SharedExtSaveData archive 0xF000000B and create the gamecoin.dat file if it doesn't
    // exist
    FileSys::Path archive_path{ptm_shared_extdata_id};
    auto archive_result{extdata_archive_factory.Open(archive_path)};
    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ERR_NOT_FORMATTED) {
        // Format the archive to create the directories
        extdata_archive_factory.Format(archive_path, FileSys::ArchiveFormatInfo());
        // Open it again to get a valid archive now that the folder exists
        auto new_archive_result{extdata_archive_factory.Open(archive_path)};
        ASSERT_MSG(new_archive_result.Succeeded(),
                   "Couldn't open the PTM SharedExtSaveData archive!");
        FileSys::Path gamecoin_path{"/gamecoin.dat"};
        auto archive{std::move(new_archive_result).Unwrap()};
        archive->CreateFile(gamecoin_path, sizeof(GameCoin));
        FileSys::Mode open_mode{};
        open_mode.write_flag.Assign(1);
        // Open the file and write the default gamecoin information
        auto gamecoin_result{archive->OpenFile(gamecoin_path, open_mode)};
        if (gamecoin_result.Succeeded()) {
            auto gamecoin{std::move(gamecoin_result).Unwrap()};
            gamecoin->Write(0, sizeof(GameCoin), true,
                            reinterpret_cast<const u8*>(&default_game_coin));
        }
    }
}

void SetPlayCoins(u16 play_coins) {
    auto nand_directory{
        FileUtil::GetUserPath(FileUtil::UserPath::NANDDir, Settings::values.nand_dir)};
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory{nand_directory, true};
    FileSys::Path archive_path{ptm_shared_extdata_id};
    auto archive_result{extdata_archive_factory.Open(archive_path)};
    ASSERT_MSG(archive_result.Succeeded(), "Couldn't open the PTM SharedExtSaveData archive!");
    auto archive{std::move(archive_result).Unwrap()};
    FileSys::Path gamecoin_path{"/gamecoin.dat"};
    FileSys::Mode open_mode{};
    open_mode.read_flag.Assign(1);
    open_mode.write_flag.Assign(1);
    // Open the file and write the gamecoin information
    auto gamecoin_result{archive->OpenFile(gamecoin_path, open_mode)};
    if (gamecoin_result.Succeeded()) {
        auto gamecoin{std::move(gamecoin_result).Unwrap()};
        GameCoin game_coin;
        gamecoin->Read(0, sizeof(GameCoin), reinterpret_cast<u8*>(&game_coin));
        game_coin.total_coins = play_coins;
        gamecoin->Write(0, sizeof(GameCoin), true, reinterpret_cast<const u8*>(&game_coin));
    }
}

Module::Interface::Interface(std::shared_ptr<Module> ptm, const char* name, u32 max_session)
    : ServiceFramework{name, max_session}, ptm{std::move(ptm)} {}

void InstallInterfaces(Core::System& system) {
    auto& service_manager{system.ServiceManager()};
    auto ptm{std::make_shared<Module>(system)};
    std::make_shared<PTM_Gets>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_Play>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_Sets>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_S>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_Sysm>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_U>(ptm)->InstallAsService(service_manager);
}

} // namespace Service::PTM
