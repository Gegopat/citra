// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/result.h"
#include "core/hle/service/ns/applet_manager.h"

namespace HLE::Applets {

class Applet {
public:
    virtual ~Applet() = default;

    /**
     * Handles a parameter from the program.
     * @param parameter Parameter data to handle.
     * @returns ResultCode Whether the operation was successful or not.
     */
    virtual ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) = 0;

    /**
     * Handles the Applet start event, triggered from the program.
     * @param parameter Parameter data to handle.
     * @returns ResultCode Whether the operation was successful or not.
     */
    ResultCode Start(const Service::APT::AppletStartupParameter& parameter);

    /// Whether the applet is currently executing instead of the host program or not.
    bool IsRunning() const;

    /// Handles an update tick for the Applet, lets it update the screen, send commands, etc.
    virtual void Update() = 0;

    virtual bool IsLibraryApplet() {
        return true;
    }

protected:
    explicit Applet(AppletId id, Service::APT::AppletManager& manager) : id{id}, manager{manager} {}

    /**
     * Handles the Applet start event, triggered from the program.
     * @param parameter Parameter data to handle.
     * @returns ResultCode Whether the operation was successful or not.
     */
    virtual ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) = 0;

    AppletId id; ///< ID of this Applet

    /// Whether this applet is currently running instead of the host program or not.
    bool is_running{};

    void SendParameter(const Service::APT::MessageParameter& parameter);

    Service::APT::AppletManager& manager;
};

} // namespace HLE::Applets
