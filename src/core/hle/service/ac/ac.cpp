// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/result.h"
#include "core/hle/service/ac/ac.h"
#include "core/hle/service/ac/ac_i.h"
#include "core/hle/service/ac/ac_u.h"
#include "core/memory.h"
#include "core/settings.h"

namespace Service::AC {

void Module::Interface::CreateDefaultConfig(Kernel::HLERequestContext& ctx) {
    std::vector<u8> buffer(sizeof(ACConfig));
    std::memcpy(buffer.data(), &ac->default_config, buffer.size());
    IPC::ResponseBuilder rb{ctx, 0x1, 1, 2};
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(buffer), 0);
    LOG_WARNING(Service_AC, "stubbed");
}

void Module::Interface::ConnectAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x4, 0, 6};
    rp.Skip(2, false); // ProcessID descriptor
    ac->connect_event = rp.PopObject<Kernel::Event>();
    if (ac->connect_event) {
        ac->connect_event->SetName("AC:connect_event");
        ac->connect_event->Signal();
        ac->ac_connected = true;
    }
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_AC, "stubbed");
}

void Module::Interface::GetConnectResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x5, 0, 2};
    rp.Skip(2, false); // ProcessID descriptor
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::CloseAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x8, 0, 4};
    rp.Skip(2, false); // ProcessID descriptor
    ac->close_event = rp.PopObject<Kernel::Event>();
    if (ac->ac_connected && ac->disconnect_event)
        ac->disconnect_event->Signal();
    if (ac->close_event) {
        ac->close_event->SetName("AC:close_event");
        ac->close_event->Signal();
    }
    ac->ac_connected = false;
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetCloseResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x9, 0, 2};
    rp.Skip(2, false); // ProcessID descriptor
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_AC, "stubbed");
}

void Module::Interface::GetWifiStatus(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0xD, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(Settings::values.n_wifi_status);
}

void Module::Interface::GetInfraPriority(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x27, 0, 2};
    const std::vector<u8>& ac_config{rp.PopStaticBuffer()};
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // Infra Priority, default 0
    LOG_WARNING(Service_AC, "stubbed");
}

void Module::Interface::SetRequestEulaVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x2D, 2, 2};
    u32 major{rp.Pop<u8>()};
    u32 minor{rp.Pop<u8>()};
    const auto& ac_config{rp.PopStaticBuffer()};
    // TODO: Copy over the input ACConfig to the stored ACConfig.
    auto rb{rp.MakeBuilder(1, 2)};
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(ac_config), 0);
    LOG_WARNING(Service_AC, "(stubbed) major={}, minor={}", major, minor);
}

void Module::Interface::RegisterDisconnectEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x30, 0, 4};
    rp.Skip(2, false); // ProcessID descriptor
    ac->disconnect_event = rp.PopObject<Kernel::Event>();
    if (ac->disconnect_event)
        ac->disconnect_event->SetName("AC:disconnect_event");
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_AC, "stubbed");
}

void Module::Interface::GetConnectingSsidLength(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 0x35, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(static_cast<u32>(ac->connected_network_name.length()));
    LOG_WARNING(Service_AC, "stubbed");
}

void Module::Interface::IsConnected(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x3E, 1, 2};
    u32 unk{rp.Pop<u32>()};
    u32 unk_descriptor{rp.Pop<u32>()};
    u32 unk_param{rp.Pop<u32>()};
    auto rb{rp.MakeBuilder(2, 0)};
    rb.Push(RESULT_SUCCESS);
    rb.Push(ac->ac_connected);
    LOG_WARNING(Service_AC, "(stubbed) unk=0x{:08X}, descriptor=0x{:08X}, param=0x{:08X}", unk,
                unk_descriptor, unk_param);
}

void Module::Interface::SetClientVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x40, 1, 2};
    u32 version{rp.Pop<u32>()};
    rp.Skip(2, false); // ProcessID descriptor
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_AC, "(stubbed) version: 0x{:08X}", version);
}

Module::Interface::Interface(std::shared_ptr<Module> ac, const char* name)
    : ServiceFramework{name}, ac{std::move(ac)} {}

void InstallInterfaces(Core::System& system) {
    auto& service_manager{system.ServiceManager()};
    auto ac{std::make_shared<Module>()};
    std::make_shared<AC_I>(ac)->InstallAsService(service_manager);
    std::make_shared<AC_U>(ac)->InstallAsService(service_manager);
}

} // namespace Service::AC
