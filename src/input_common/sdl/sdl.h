// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "core/frontend/input.h"

union SDL_Event;
namespace Common {
class ParamPackage;
} // namespace Common
namespace InputCommon::Polling {
class DevicePoller;
enum class DeviceType;
} // namespace InputCommon::Polling

namespace InputCommon::SDL {

/// Initializes and registers SDL device factories
void Init();

/// Unregisters SDL device factories and shut them down.
void Shutdown();

/// Call after SDL_INIT and before the event poll loop to open all connected joysticks
void InitSDLJoysticks();

/// Needs to be called before SDL_QuitSubSystem.
void CloseSDLJoysticks();

/// Handle SDL_Events for joysticks from SDL_PollEvent
void HandleGameControllerEvent(const SDL_Event& event);

/// A Loop that calls HandleGameControllerEvent until Shutdown is called
void PollLoop();

/// Creates a ParamPackage from an SDL_Event that can directly be used to create a ButtonDevice
Common::ParamPackage SDLEventToButtonParamPackage(const SDL_Event& event);

namespace Polling {

/// Get all DevicePoller that use the SDL backend for a specific device type
void GetPollers(InputCommon::Polling::DeviceType type,
                std::vector<std::unique_ptr<InputCommon::Polling::DevicePoller>>& pollers);

} // namespace Polling
} // namespace InputCommon::SDL
