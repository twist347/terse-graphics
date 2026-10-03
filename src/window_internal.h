#pragma once

struct GLFWwindow;

namespace tgx {
    class Window;
}

// What Device needs from Window without knowing the windowing backend.
namespace tgx::detail {
    // Signature-compatible with glfwGetProcAddress and glad's GLADloadfunc.
    using GlProc = void (*)();
    using GlLoader = GlProc (*)(const char *name);

    // Resolves GL functions for the window's context; it must be current.
    [[nodiscard]] auto gl_loader(const Window &window) noexcept -> GlLoader;

    // Shows what was drawn into the window's context: the Device presents,
    // the window only lends it the surface.
    auto swap_buffers(GLFWwindow *window) noexcept -> void;
}
