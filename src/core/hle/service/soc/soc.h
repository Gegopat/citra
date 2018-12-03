// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include "core/hle/service/service.h"

namespace Core {
class System;
} // namespace Core

namespace Service::SOC {

/// Holds information about a particular socket
struct SocketHolder {
    u32 socket_fd; ///< The socket descriptor
    bool blocking; ///< Whether the socket is blocking or not, it's only read on Windows.
};

class Module final {
public:
    Module();
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> soc, const char* name);
        ~Interface();

    protected:
        void Socket(Kernel::HLERequestContext& ctx);
        void Bind(Kernel::HLERequestContext& ctx);
        void Fcntl(Kernel::HLERequestContext& ctx);
        void Listen(Kernel::HLERequestContext& ctx);
        void Accept(Kernel::HLERequestContext& ctx);
        void GetHostID(Kernel::HLERequestContext& ctx);
        void Close(Kernel::HLERequestContext& ctx);
        void SendTo(Kernel::HLERequestContext& ctx);
        void RecvFromOther(Kernel::HLERequestContext& ctx);
        void RecvFrom(Kernel::HLERequestContext& ctx);
        void Poll(Kernel::HLERequestContext& ctx);
        void GetSockName(Kernel::HLERequestContext& ctx);
        void Shutdown(Kernel::HLERequestContext& ctx);
        void GetPeerName(Kernel::HLERequestContext& ctx);
        void Connect(Kernel::HLERequestContext& ctx);
        void InitializeSockets(Kernel::HLERequestContext& ctx);
        void ShutdownSockets(Kernel::HLERequestContext& ctx);
        void CloseSockets(Kernel::HLERequestContext& ctx);
        void GetSockOpt(Kernel::HLERequestContext& ctx);
        void SetSockOpt(Kernel::HLERequestContext& ctx);
        void CleanupSockets();

    private:
        std::shared_ptr<Module> soc;
    };

private:
    /// Holds info about the currently open sockets
    std::unordered_map<u32, SocketHolder> open_sockets;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::SOC
