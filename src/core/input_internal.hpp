#pragma once

struct GLFWwindow;

// How the Window drives input. State and callbacks live in input.cpp.
namespace tgx::detail {
    // For a new window: sets the callbacks, reads the mouse position.
    auto attach_input(GLFWwindow *handle) noexcept -> void;

    // Takes the cursor's position as is, without a movement.
    auto reset_mouse(GLFWwindow *handle) noexcept -> void;

    // After a cursor mode change: reset_mouse(), and the next poll's cursor
    // moves count as no movement. GLFW moves the cursor itself (into the
    // middle on capture, back on release) and reports it in that poll.
    auto settle_mouse(GLFWwindow *handle) noexcept -> void;

    // Clears all state before the window is destroyed. The callbacks go
    // with the window.
    auto detach_input() noexcept -> void;

    // Clears the previous poll's events (presses, releases, mouse movement,
    // wheel, text). Called right before polling.
    auto begin_input_frame() noexcept -> void;

    // Called right after polling.
    auto end_input_frame() noexcept -> void;
}
