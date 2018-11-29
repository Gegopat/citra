// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/logging/log.h"
#include "video_core/pica.h"
#include "video_core/renderer/renderer.h"
#include "video_core/video_core.h"

namespace VideoCore {

std::unique_ptr<Renderer> g_renderer;

std::atomic_bool g_hw_shaders_enabled;
std::atomic_bool g_hw_shaders_accurate_gs;
std::atomic_bool g_hw_shaders_accurate_mul;
std::atomic_bool g_bg_color_update_requested;
std::atomic_bool g_screenshot_requested;
void* g_screenshot_bits;
std::function<void()> g_screenshot_complete_callback;
Layout::FramebufferLayout g_screenshot_framebuffer_layout;

/// Initialize the video core
Core::System::ResultStatus Init(Core::System& system) {
    Pica::Init();
    g_renderer = std::make_unique<Renderer>(system);
    auto result{g_renderer->Init()};
    if (result != Core::System::ResultStatus::Success)
        LOG_ERROR(Render, "initialization failed!");
    else
        LOG_DEBUG(Render, "initialized OK");
    return result;
}

/// Shutdown the video core
void Shutdown() {
    Pica::Shutdown();
    g_renderer.reset();
    LOG_DEBUG(Render, "shutdown OK");
}

void RequestScreenshot(void* data, std::function<void()> callback,
                       const Layout::FramebufferLayout& layout) {
    if (g_screenshot_requested) {
        LOG_ERROR(Render, "A screenshot is already requested or in progress, ignoring the request");
        return;
    }
    g_screenshot_bits = data;
    g_screenshot_complete_callback = std::move(callback);
    g_screenshot_framebuffer_layout = layout;
    g_screenshot_requested = true;
}

} // namespace VideoCore
