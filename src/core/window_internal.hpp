#pragma once

#include "tgx/core/math.hpp"

// The window, for the rest of tgx, without the windowing backend.
namespace tgx::detail {
    // Signature-compatible with glfwGetProcAddress and glad's GLADloadfunc.
    using GlProc = void (*)();
    using GlLoader = GlProc (*)(const char *name);

    // For the window's context, which is current.
    [[nodiscard]] auto gl_loader() noexcept -> GlLoader;

    // The Device presents; the window only provides the surface.
    auto swap_buffers() noexcept -> void;

    // As last reported. Querying can be a round trip to the X11 server.
    [[nodiscard]] auto window_size() noexcept -> Size;

    [[nodiscard]] auto framebuffer_size() noexcept -> Size;
}
