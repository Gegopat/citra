// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/ps/ps_ps.h"

namespace Service::PS {

void PS_PS::GetLocalFriendCodeSeed(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0xA, 3, 0};
    rb.Push(RESULT_SUCCESS);
    auto [exists, lfcs]{GetLocalFriendCodeSeedTuple()};
    rb.Push<u64>(lfcs.seed);
}

PS_PS::PS_PS() : ServiceFramework{"ps:ps"} {
    static const FunctionInfo functions[]{
        {0x00010244, nullptr, "SignRsaSha256"},
        {0x00020244, nullptr, "VerifyRsaSha256"},
        {0x00040204, nullptr, "EncryptDecryptAes"},
        {0x00050284, nullptr, "EncryptSignDecryptVerifyAesCcm"},
        {0x00060040, nullptr, "GetRomID"},
        {0x00070040, nullptr, "GetRomID2"},
        {0x00080040, nullptr, "GetRomMakerCode"},
        {0x00090000, nullptr, "GetCTRCardAutoStartupBit"},
        {0x000A0000, &PS_PS::GetLocalFriendCodeSeed, "GetLocalFriendCodeSeed"},
        {0x000B0000, nullptr, "GetDeviceID"},
        {0x000C0000, nullptr, "SeedRNG"},
        {0x000D0042, nullptr, "GenerateRandomBytes"},
        {0x000E0082, nullptr, "InterfaceForPXI_0x04010084"},
        {0x000F0082, nullptr, "InterfaceForPXI_0x04020082"},
        {0x00100042, nullptr, "InterfaceForPXI_0x04030044"},
        {0x00110042, nullptr, "InterfaceForPXI_0x04040044"},
    };
    RegisterHandlers(functions);
};

void InstallInterfaces(Core::System& system) {
    auto& service_manager{system.ServiceManager()};
    std::make_shared<PS_PS>()->InstallAsService(service_manager);
}

std::tuple<bool, LocalFriendCodeSeed> GetLocalFriendCodeSeedTuple() {
    const auto path{fmt::format("{}LocalFriendCodeSeed_B",
                                FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir))};
    if (FileUtil::Exists(path)) {
        LocalFriendCodeSeed lfcs;
        FileUtil::IOFile file{path, "rb"};
        file.ReadBytes(&lfcs, sizeof(LocalFriendCodeSeed));
        return std::make_tuple(true, lfcs);
    } else {
        LOG_WARNING(Service_PS, "LocalFriendCodeSeed_B not found");
        LocalFriendCodeSeed lfcs{};
        return std::make_tuple(false, lfcs);
    }
}

} // namespace Service::PS
