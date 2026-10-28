#pragma once

#include "tgx/core/math.hpp"

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

    // The math under Input::axis and direction, here so tests can use it.

    // -1, 0 or 1; 0 when neither is held or both are.
    [[nodiscard]] constexpr auto unit_axis(bool negative, bool positive) noexcept -> float {
        return static_cast<float>(positive) - static_cast<float>(negative);
    }

    // 0 when none is held or opposites cancel.
    [[nodiscard]] inline auto unit_direction(bool left, bool right, bool up, bool down) noexcept -> Vec2 {
        const Vec2 direction{unit_axis(left, right), unit_axis(up, down)};
        return direction == Vec2{} ? direction : normalize(direction);
    }
}
