#pragma once

struct GLFWwindow;

namespace tgx {
    class Input;
}

// The one Input, attached to the window at a stable address across moves.
namespace tgx::detail {
    [[nodiscard]] auto input() noexcept -> Input &;

    // Installs callbacks and seeds the mouse position; detach clears state.
    auto attach_input(GLFWwindow *handle) noexcept -> void;
    auto detach_input() noexcept -> void;

    auto begin_input_frame() noexcept -> void;
    auto end_input_frame() noexcept -> void;

    auto settle_mouse(GLFWwindow *handle) noexcept -> void;
}
