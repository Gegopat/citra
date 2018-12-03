// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/soc/soc_u.h"

namespace Service::SOC {

SOC_U::SOC_U(std::shared_ptr<Module> soc) : Module::Interface{std::move(soc), "soc:U"} {
    static const FunctionInfo functions[]{
        {0x00010044, &SOC_U::InitializeSockets, "InitializeSockets"},
        {0x000200C2, &SOC_U::Socket, "Socket"},
        {0x00030082, &SOC_U::Listen, "Listen"},
        {0x00040082, &SOC_U::Accept, "Accept"},
        {0x00050084, &SOC_U::Bind, "Bind"},
        {0x00060084, &SOC_U::Connect, "Connect"},
        {0x00070104, nullptr, "recvfrom_other"},
        {0x00080102, &SOC_U::RecvFrom, "RecvFrom"},
        {0x00090106, nullptr, "sendto_other"},
        {0x000A0106, &SOC_U::SendTo, "SendTo"},
        {0x000B0042, &SOC_U::Close, "Close"},
        {0x000C0082, &SOC_U::Shutdown, "Shutdown"},
        {0x000D0082, nullptr, "GetHostByName"},
        {0x000E00C2, nullptr, "GetHostByAddr"},
        {0x000F0106, nullptr, "GetAddrInfo"},
        {0x00100102, nullptr, "GetNameInfo"},
        {0x00110102, &SOC_U::GetSockOpt, "GetSockOpt"},
        {0x00120104, &SOC_U::SetSockOpt, "SetSockOpt"},
        {0x001300C2, &SOC_U::Fcntl, "Fcntl"},
        {0x00140084, &SOC_U::Poll, "Poll"},
        {0x00150042, nullptr, "SockAtMark"},
        {0x00160000, &SOC_U::GetHostID, "GetHostID"},
        {0x00170082, &SOC_U::GetSockName, "GetSockName"},
        {0x00180082, &SOC_U::GetPeerName, "GetPeerName"},
        {0x00190000, &SOC_U::ShutdownSockets, "ShutdownSockets"},
        {0x001A00C0, nullptr, "GetNetworkOpt"},
        {0x001B0040, nullptr, "ICMPSocket"},
        {0x001C0104, nullptr, "ICMPPing"},
        {0x001D0040, nullptr, "ICMPCancel"},
        {0x001E0040, nullptr, "ICMPClose"},
        {0x001F0040, nullptr, "GetResolverInfo"},
        {0x00210002, &SOC_U::CloseSockets, "CloseSockets"},
        {0x00230040, nullptr, "AddGlobalSocket"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::SOC
