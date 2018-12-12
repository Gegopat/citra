// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/core.h"
#include "core/frontend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/nwm/nwm_ext.h"
#include "core/settings.h"

namespace Service::NWM {

NWM_EXT::NWM_EXT(Core::System& system) : ServiceFramework{"nwm::EXT"}, system{system} {
    static const FunctionInfo functions[]{
        {0x00080040, &NWM_EXT::ControlWirelessEnabled, "ControlWirelessEnabled"},
    };
    RegisterHandlers(functions);
}

NWM_EXT::~NWM_EXT() = default;

void NWM_EXT::ControlWirelessEnabled(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x0008, 1, 0};
    u8 enabled{rp.Pop<u8>()};
    auto result{RESULT_SUCCESS};
    switch (enabled) {
    case 0: { // Enable
        auto& shared_page{system.Kernel().GetSharedPageHandler()};
        if (shared_page.GetNetworkState() == SharedPage::NetworkState::Internet) {
            result =
                ResultCode(13, ErrorModule::NWM, ErrorSummary::InvalidState, ErrorLevel::Status);
            break;
        }
        Settings::values.n_wifi_status = system.ServiceManager()
                                                 .GetService<CFG::Module::Interface>("cfg:u")
                                                 ->GetModule()
                                                 ->GetNewModel()
                                             ? 2
                                             : 1;
        Settings::values.n_wifi_link_level = 3;
        Settings::values.n_state = 2;
        shared_page.SetWifiLinkLevel(SharedPage::WifiLinkLevel::Best);
        shared_page.SetNetworkState(SharedPage::NetworkState::Internet);
        system.GetFrontend().UpdateNetwork();
        break;
    }
    case 1: { // Disable
        auto& shared_page{system.Kernel().GetSharedPageHandler()};
        if (shared_page.GetNetworkState() != SharedPage::NetworkState::Internet) {
            result =
                ResultCode(13, ErrorModule::NWM, ErrorSummary::InvalidState, ErrorLevel::Status);
            break;
        }
        Settings::values.n_wifi_status = 0;
        Settings::values.n_wifi_link_level = 0;
        Settings::values.n_state = 7;
        shared_page.SetWifiLinkLevel(SharedPage::WifiLinkLevel::Off);
        shared_page.SetNetworkState(SharedPage::NetworkState::Disabled);
        system.GetFrontend().UpdateNetwork();
        break;
    }
    default: {
        LOG_ERROR(Service_NWM, "Invalid enabled value {}", enabled);
        break;
    }
    }
    auto rb{rp.MakeBuilder(1, 0)};
    rb.Push(result);
}

} // namespace Service::NWM
