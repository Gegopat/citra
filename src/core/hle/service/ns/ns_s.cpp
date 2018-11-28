// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/ns/ns.h"
#include "core/hle/service/ns/ns_s.h"
#include "core/loader/loader.h"
#include "core/settings.h"

namespace Service::NS {

void NS_S::LaunchTitle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x2, 3, 0};
    u64 program_id{rp.Pop<u64>()};
    u32 flags{rp.Pop<u32>()};
    auto media_type{(program_id == 0) ? FS::MediaType::GameCard
                                      : AM::GetProgramMediaType(program_id)};
    if (!Settings::values.enable_ns_launch) {
        auto rb{rp.MakeBuilder(2, 0)};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(0);
    } else {
        auto process{Launch(system, media_type, program_id)};
        auto rb{rp.MakeBuilder(2, 0)};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(process ? process->process_id : 0);
    }
    LOG_DEBUG(Service_NS, "program_id={}, media_type={}, flags={}", program_id,
              static_cast<u32>(media_type), flags);
}

void NS_S::ShutdownAsync(Kernel::HLERequestContext& ctx) {
    system.CloseProgram();
    IPC::ResponseBuilder rb{ctx, 0xE, 1, 0};
    rb.Push(RESULT_SUCCESS);
}

void NS_S::RebootSystemClean(Kernel::HLERequestContext& ctx) {
    system.Restart();
    IPC::ResponseBuilder rb{ctx, 0x16, 1, 0};
    rb.Push(RESULT_SUCCESS);
}

NS_S::NS_S(Core::System& system) : ServiceFramework{"ns:s", 2}, system{system} {
    static const FunctionInfo functions[]{
        {0x000100C0, nullptr, "LaunchFIRM"},
        {0x000200C0, &NS_S::LaunchTitle, "LaunchTitle"},
        {0x00030000, nullptr, "TerminateProgram"},
        {0x00040040, nullptr, "TerminateProcess"},
        {0x000500C0, nullptr, "LaunchProgramFIRM"},
        {0x00060042, nullptr, "SetFIRMParams4A0"},
        {0x00070042, nullptr, "CardUpdateInitialize"},
        {0x00080000, nullptr, "CardUpdateShutdown"},
        {0x000D0140, nullptr, "SetTWLBannerHMAC"},
        {0x000E0000, &NS_S::ShutdownAsync, "ShutdownAsync"},
        {0x00100180, nullptr, "RebootSystem"},
        {0x00110100, nullptr, "TerminateTitle"},
        {0x001200C0, nullptr, "SetProgramCpuTimeLimit"},
        {0x00150140, nullptr, "LaunchProgram"},
        {0x00160000, &NS_S::RebootSystemClean, "RebootSystemClean"},
    };
    RegisterHandlers(functions);
}

NS_S::~NS_S() = default;

} // namespace Service::NS
