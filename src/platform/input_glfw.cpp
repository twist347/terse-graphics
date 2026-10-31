#include "platform/input_glfw.hpp"

#include "tgx/core/input.hpp"
#include "tgx/core/math.hpp"

#include "platform/glfw_keys.hpp"
#include "platform/input_internal.hpp"

#include <GLFW/glfw3.h>

#include <optional>

namespace {
    [[nodiscard]] auto to_action(int action) noexcept -> std::optional<tgx::detail::InputAction> {
        using enum tgx::detail::InputAction;
        switch (action) {
            case GLFW_PRESS: return press;
            case GLFW_REPEAT: return repeat;
            case GLFW_RELEASE: return release;
            default: return std::nullopt;
        }
    }

    [[nodiscard]] auto mouse_position(GLFWwindow *handle) noexcept -> tgx::Vec2 {
        double x = 0.0, y = 0.0;
        glfwGetCursorPos(handle, &x, &y);
        return {static_cast<float>(x), static_cast<float>(y)};
    }

    auto on_key(GLFWwindow *, int code, int, int action, int) noexcept -> void {
        if (const auto translated = to_action(action)) {
            tgx::detail::InputAccess::key(tgx::detail::input(), tgx::detail::to_key(code), *translated);
        }
    }

    auto on_mouse_button(GLFWwindow *, int button, int action, int) noexcept -> void {
        // GLFW uses the same order as MouseButton.
        if (button >= 0 && button < tgx::mouse_button_count) {
            if (const auto translated = to_action(action)) {
                tgx::detail::InputAccess::button(tgx::detail::input(), static_cast<tgx::MouseButton>(button), *translated);
            }
        }
    }

    auto on_cursor(GLFWwindow *, double x, double y) noexcept -> void {
        tgx::detail::InputAccess::cursor(tgx::detail::input(), {static_cast<float>(x), static_cast<float>(y)});
    }

    auto on_scroll(GLFWwindow *, double x, double y) noexcept -> void {
        tgx::detail::InputAccess::scroll(tgx::detail::input(), {static_cast<float>(x), static_cast<float>(y)});
    }

    auto on_char(GLFWwindow *, unsigned int c) noexcept -> void {
        tgx::detail::InputAccess::character(tgx::detail::input(), static_cast<char32_t>(c));
    }
}

namespace tgx::detail {
    auto input() noexcept -> Input & {
        static auto state = InputAccess::create();
        return state;
    }

    auto attach_input(GLFWwindow *handle) noexcept -> void {
        InputAccess::reset(input(), mouse_position(handle));

        glfwSetKeyCallback(handle, on_key);
        glfwSetMouseButtonCallback(handle, on_mouse_button);
        glfwSetCursorPosCallback(handle, on_cursor);
        glfwSetScrollCallback(handle, on_scroll);
        glfwSetCharCallback(handle, on_char);
    }

    auto detach_input() noexcept -> void {
        InputAccess::reset(input());
    }

    auto begin_input_frame() noexcept -> void {
        InputAccess::begin_frame(input());
    }

    auto end_input_frame() noexcept -> void {
        InputAccess::end_frame(input());
    }

    auto settle_mouse(GLFWwindow *handle) noexcept -> void {
        InputAccess::settle_mouse(input(), mouse_position(handle));
    }
}
