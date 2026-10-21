#include "tgx/core/input.hpp"

#include "tgx/core/assert.hpp"

#include "core/geometry.hpp"
#include "core/glfw_keys.hpp"
#include "core/input_internal.hpp"
#include "core/utf8.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace {
    // Per key or button. Edges are stored, not derived from down, so a tap
    // shorter than a frame still shows.
    enum : std::uint8_t {
        bit_down = 1,
        bit_pressed = 2,
        bit_released = 4,
        // Pressed or OS-repeated since the previous poll.
        bit_repeated = 8,
    };

    // Filled by GLFW callbacks during a poll.
    struct InputState {
        std::array<std::uint8_t, tgx::key_count> keys{};
        std::array<std::uint8_t, tgx::mouse_button_count> buttons{};
        tgx::Vec2 mouse{};
        tgx::Vec2 mouse_delta{};
        // Through the poll after a cursor mode change: the cursor moves, but
        // the move is GLFW's, not the player's.
        bool settling{false};
        tgx::Vec2 wheel{};
        std::string text{};
    };

    InputState s_input{};

    // Asserted: a value cast from outside the enum would read past the
    // array.
    [[nodiscard]] auto bits_of(tgx::Key key) noexcept -> std::uint8_t {
        TGX_ASSERT_MSG(std::to_underlying(key) < tgx::key_count, "key {}: not a Key", std::to_underlying(key));

        return s_input.keys[std::to_underlying(key)];
    }

    [[nodiscard]] auto bits_of(tgx::MouseButton button) noexcept -> std::uint8_t {
        TGX_ASSERT_MSG(
            std::to_underlying(button) < tgx::mouse_button_count,
            "mouse button {}: not a MouseButton",
            std::to_underlying(button)
        );

        return s_input.buttons[std::to_underlying(button)];
    }

    // A repeat is not a press and does not change down. Only repeated() sees
    // it.
    auto set_edge(std::uint8_t &bits, int action) noexcept -> void {
        if (action == GLFW_PRESS) {
            bits |= bit_down | bit_pressed | bit_repeated;
        } else if (action == GLFW_REPEAT) {
            bits |= bit_repeated;
        } else if (action == GLFW_RELEASE) {
            bits = static_cast<std::uint8_t>((bits & ~bit_down) | bit_released);
        }
    }

    auto on_key(GLFWwindow *, int code, int, int action, int) noexcept -> void {
        if (const tgx::Key key = tgx::detail::to_key(code); key != tgx::Key::unknown) {
            set_edge(s_input.keys[std::to_underlying(key)], action);
        }
    }

    auto on_mouse_button(GLFWwindow *, int button, int action, int) noexcept -> void {
        // GLFW uses the same order as MouseButton.
        if (button >= 0 && button < tgx::mouse_button_count) {
            set_edge(s_input.buttons[static_cast<std::size_t>(button)], action);
        }
    }

    auto on_cursor(GLFWwindow *, double x, double y) noexcept -> void {
        const tgx::Vec2 mouse{static_cast<float>(x), static_cast<float>(y)};
        if (!s_input.settling) {
            s_input.mouse_delta += mouse - s_input.mouse;
        }
        s_input.mouse = mouse;
    }

    auto on_scroll(GLFWwindow *, double x, double y) noexcept -> void {
        s_input.wheel += {static_cast<float>(x), static_cast<float>(y)};
    }

    auto on_char(GLFWwindow *, unsigned int c) noexcept -> void {
        tgx::detail::append_utf8(s_input.text, static_cast<char32_t>(c));
    }
}

namespace tgx {
    auto Input::down(Key key) const noexcept -> bool {
        return (bits_of(key) & bit_down) != 0;
    }

    auto Input::pressed(Key key) const noexcept -> bool {
        return (bits_of(key) & bit_pressed) != 0;
    }

    auto Input::released(Key key) const noexcept -> bool {
        return (bits_of(key) & bit_released) != 0;
    }

    auto Input::repeated(Key key) const noexcept -> bool {
        return (bits_of(key) & bit_repeated) != 0;
    }

    auto Input::axis(Key negative, Key positive) const noexcept -> float {
        return detail::unit_axis(down(negative), down(positive));
    }

    auto Input::direction() const noexcept -> Vec2 {
        const auto held = [this](Key a, Key b) noexcept { return down(a) || down(b); };
        return detail::unit_direction(
            held(Key::a, Key::left), held(Key::d, Key::right),
            held(Key::w, Key::up), held(Key::s, Key::down)
        );
    }

    auto Input::direction(Key left, Key right, Key up, Key down) const noexcept -> Vec2 {
        return detail::unit_direction(this->down(left), this->down(right), this->down(up), this->down(down));
    }

    auto Input::down(MouseButton button) const noexcept -> bool {
        return (bits_of(button) & bit_down) != 0;
    }

    auto Input::pressed(MouseButton button) const noexcept -> bool {
        return (bits_of(button) & bit_pressed) != 0;
    }

    auto Input::released(MouseButton button) const noexcept -> bool {
        return (bits_of(button) & bit_released) != 0;
    }

    auto Input::mouse() const noexcept -> Vec2 {
        return s_input.mouse;
    }

    auto Input::mouse_delta() const noexcept -> Vec2 {
        return s_input.mouse_delta;
    }

    auto Input::wheel() const noexcept -> Vec2 {
        return s_input.wheel;
    }

    auto Input::text() const noexcept -> std::string_view {
        return s_input.text;
    }

    auto detail::attach_input(GLFWwindow *handle) noexcept -> void {
        s_input = {};
        // So the first move is not a jump from the corner.
        reset_mouse(handle);

        glfwSetKeyCallback(handle, on_key);
        glfwSetMouseButtonCallback(handle, on_mouse_button);
        glfwSetCursorPosCallback(handle, on_cursor);
        glfwSetScrollCallback(handle, on_scroll);
        glfwSetCharCallback(handle, on_char);
    }

    auto detail::reset_mouse(GLFWwindow *handle) noexcept -> void {
        double x = 0.0, y = 0.0;
        glfwGetCursorPos(handle, &x, &y);
        s_input.mouse = {static_cast<float>(x), static_cast<float>(y)};
    }

    auto detail::settle_mouse(GLFWwindow *handle) noexcept -> void {
        reset_mouse(handle);
        s_input.settling = true;
    }

    auto detail::detach_input() noexcept -> void {
        s_input = {};
    }

    auto detail::begin_input_frame() noexcept -> void {
        // Held keys and buttons stay down.
        for (std::uint8_t &bits: s_input.keys) {
            bits &= bit_down;
        }
        for (std::uint8_t &bits: s_input.buttons) {
            bits &= bit_down;
        }
        s_input.mouse_delta = {};
        s_input.wheel = {};
        s_input.text.clear();
    }

    auto detail::end_input_frame() noexcept -> void {
        s_input.settling = false;
    }
}
