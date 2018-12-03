// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/soc/soc_p.h"

namespace Service::SOC {

SOC_P::SOC_P(std::shared_ptr<Module> soc) : Module::Interface{std::move(soc), "soc:P"} {
    static const FunctionInfo functions[]{
        {0x00010044, &SOC_P::InitializeSockets, "InitializeSockets"},
        {0x000200C2, &SOC_P::Socket, "Socket"},
        {0x00030082, &SOC_P::Listen, "Listen"},
        {0x00040082, &SOC_P::Accept, "Accept"},
        {0x00050084, &SOC_P::Bind, "Bind"},
        {0x00060084, &SOC_P::Connect, "Connect"},
        {0x00070104, nullptr, "recvfrom_other"},
        {0x00080102, &SOC_P::RecvFrom, "RecvFrom"},
        {0x00090106, nullptr, "sendto_other"},
        {0x000A0106, &SOC_P::SendTo, "SendTo"},
        {0x000B0042, &SOC_P::Close, "Close"},
        {0x000C0082, &SOC_P::Shutdown, "Shutdown"},
        {0x000D0082, nullptr, "GetHostByName"},
        {0x000E00C2, nullptr, "GetHostByAddr"},
        {0x000F0106, nullptr, "GetAddrInfo"},
        {0x00100102, nullptr, "GetNameInfo"},
        {0x00110102, &SOC_P::GetSockOpt, "GetSockOpt"},
        {0x00120104, &SOC_P::SetSockOpt, "SetSockOpt"},
        {0x001300C2, &SOC_P::Fcntl, "Fcntl"},
        {0x00140084, &SOC_P::Poll, "Poll"},
        {0x00150042, nullptr, "SockAtMark"},
        {0x00160000, &SOC_P::GetHostID, "GetHostID"},
        {0x00170082, &SOC_P::GetSockName, "GetSockName"},
        {0x00180082, &SOC_P::GetPeerName, "GetPeerName"},
        {0x00190000, &SOC_P::ShutdownSockets, "ShutdownSockets"},
        {0x001A00C0, nullptr, "GetNetworkOpt"},
        {0x001B0040, nullptr, "ICMPSocket"},
        {0x001C0104, nullptr, "ICMPPing"},
        {0x001D0040, nullptr, "ICMPCancel"},
        {0x001E0040, nullptr, "ICMPClose"},
        {0x001F0040, nullptr, "GetResolverInfo"},
        {0x00210002, &SOC_P::CloseSockets, "CloseSockets"},
        {0x00230040, nullptr, "AddGlobalSocket"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::SOC
