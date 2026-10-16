#pragma once

#include "tgx/core/math.hpp"

#include <cstdint>
#include <string_view>

namespace tgx {
    class Window;

    enum class Key : std::uint8_t {
        unknown,

        a, b, c, d, e, f, g, h, i, j, k, l, m,
        n, o, p, q, r, s, t, u, v, w, x, y, z,

        digit0, digit1, digit2, digit3, digit4, digit5, digit6, digit7, digit8, digit9,

        space, apostrophe, comma, minus, period, slash, semicolon, equal,
        left_bracket, backslash, right_bracket, grave,

        escape, enter, tab, backspace, insert, del,
        right, left, down, up,
        page_up, page_down, home, end,
        caps_lock, scroll_lock, num_lock, print_screen, pause,

        f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12,

        keypad0, keypad1, keypad2, keypad3, keypad4, keypad5, keypad6, keypad7, keypad8, keypad9,
        keypad_decimal, keypad_divide, keypad_multiply, keypad_subtract,
        keypad_add, keypad_enter, keypad_equal,

        left_shift, left_control, left_alt, left_super,
        right_shift, right_control, right_alt, right_super,
        menu,
    };

    inline constexpr int key_count = static_cast<int>(Key::menu) + 1;

    enum class MouseButton : std::uint8_t {
        left,
        right,
        middle,
        // Side buttons: back and forward in browsers.
        x1,
        x2,
    };

    inline constexpr int mouse_button_count = static_cast<int>(MouseButton::x2) + 1;

    // Keyboard and mouse as of the last Window::poll_events(). Get it with
    // window().input() or app->input(). Its state lives inside tgx.
    //
    //     const auto &input = app->input();
    //     if (input.down(tgx::Key::d)) { player.x += speed * dt; }
    //     if (input.pressed(tgx::MouseButton::left)) { spawn(world.to_world(input.mouse())); }
    class Input {
    public:
        Input(const Input &) = delete;
        auto operator=(const Input &) -> Input & = delete;

        Input(Input &&) = delete;
        auto operator=(Input &&) -> Input & = delete;

        [[nodiscard]] auto down(Key key) const noexcept -> bool;

        // Went down or up since the previous poll. A tap within one frame is
        // both, without being down, so it is not lost.
        [[nodiscard]] auto pressed(Key key) const noexcept -> bool;
        [[nodiscard]] auto released(Key key) const noexcept -> bool;

        // Pressed, or repeated by the OS while held, as when typing. For
        // Backspace in a text field or stepping through a menu.
        [[nodiscard]] auto repeated(Key key) const noexcept -> bool;

        // Held keys along one line: -1 for negative, 1 for positive, 0 for
        // neither or both.
        //
        //     paddle.y += input.axis(tgx::Key::w, tgx::Key::s) * speed * dt;
        [[nodiscard]] auto axis(Key negative, Key positive) const noexcept -> float;

        // Held WASD or arrow keys, length 1 or 0, y down. Diagonals are as
        // fast as straight lines; opposite keys cancel out.
        //
        //     player += input.direction() * speed * dt;
        [[nodiscard]] auto direction() const noexcept -> Vec2;

        // Like direction(), with your own four keys.
        [[nodiscard]] auto direction(Key left, Key right, Key up, Key down) const noexcept -> Vec2;

        [[nodiscard]] auto down(MouseButton button) const noexcept -> bool;
        [[nodiscard]] auto pressed(MouseButton button) const noexcept -> bool;
        [[nodiscard]] auto released(MouseButton button) const noexcept -> bool;

        // In screen coordinates, as the Canvas: (0, 0) top-left, y down.
        // Canvas::to_world converts it to a world point.
        [[nodiscard]] auto mouse() const noexcept -> Vec2;

        // Since the previous poll.
        [[nodiscard]] auto mouse_delta() const noexcept -> Vec2;

        // Since the previous poll. y > 0 is away from you, x sideways
        // (touchpad). One wheel notch is about 1.
        [[nodiscard]] auto wheel() const noexcept -> Vec2;

        // Typed since the previous poll, UTF-8, with layout and Shift
        // applied. For text fields.
        [[nodiscard]] auto text() const noexcept -> std::string_view;

    private:
        friend class Window;

        Input() noexcept = default;
    };
}
