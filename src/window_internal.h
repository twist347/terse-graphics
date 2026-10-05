#pragma once

#include "tgx/math.h"

// What the rest of tgx needs from the one window without knowing the
// windowing backend.
namespace tgx::detail {
    // Signature-compatible with glfwGetProcAddress and glad's GLADloadfunc.
    using GlProc = void (*)();
    using GlLoader = GlProc (*)(const char *name);

    // Resolves GL functions for the window's context, which is current.
    [[nodiscard]] auto gl_loader() noexcept -> GlLoader;

    // Shows what was drawn into the window's context: the Device presents,
    // the window only lends it the surface.
    auto swap_buffers() noexcept -> void;

    // As the backend last reported them; asking it instead can be a round
    // trip to the display server (X11).
    [[nodiscard]] auto window_size() noexcept -> Size;

    [[nodiscard]] auto framebuffer_size() noexcept -> Size;
}
