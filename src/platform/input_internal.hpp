#pragma once

#include "tgx/core/input.hpp"
#include "tgx/core/math.hpp"

// Input updates in tgx's own types, apart from GLFW.
namespace tgx::detail {
    enum class InputAction {
        press,
        repeat,
        release,
    };

    // The platform writes; the app only reads Input. No window is needed to
    // create an instance and check its transitions here.
    struct InputAccess {
        [[nodiscard]] static auto create() noexcept -> Input { return Input{}; }

        // A fresh window's state, with the initial mouse position as is.
        static auto reset(Input &input, Vec2 mouse = {}) noexcept -> void;

        // Before polling: clear events, keeping held keys and buttons.
        static auto begin_frame(Input &input) noexcept -> void;
        static auto end_frame(Input &input) noexcept -> void;

        static auto key(Input &input, Key key, InputAction action) noexcept -> void;
        static auto button(Input &input, MouseButton button, InputAction action) noexcept -> void;
        static auto cursor(Input &input, Vec2 position) noexcept -> void;
        static auto scroll(Input &input, Vec2 offset) noexcept -> void;
        static auto character(Input &input, char32_t codepoint) -> void;

        // GLFW moves the cursor on a mode change. Take the position as is
        // and suppress movement through the next poll, still tracking it.
        static auto settle_mouse(Input &input, Vec2 position) noexcept -> void;
    };

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
