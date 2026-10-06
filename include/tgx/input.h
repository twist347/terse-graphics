#pragma once

#include "tgx/math.h"

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
        // The side buttons, back and forward in browsers.
        x1,
        x2,
    };

    inline constexpr int mouse_button_count = static_cast<int>(MouseButton::x2) + 1;

    // Keyboard and mouse as of the last Window::poll_events(). Reached through
    // window().input() or app->input(); like the Window, it only reads state
    // kept in one place inside tgx.
    //
    //     const auto &input = app->input();
    //     if (input.down(tgx::Key::d)) { player.x += speed * dt; }
    //     if (input.pressed(tgx::MouseButton::left)) { spawn(world.to_world(input.mouse())); }
    class Input {
    public:
        Input(const Input &) = delete;
        auto operator=(const Input &) -> Input & = delete;

        [[nodiscard]] auto down(Key key) const noexcept -> bool;

        // Went down, or up, since the poll before. A tap within one frame is
        // both pressed and released without being down, so it is not lost.
        [[nodiscard]] auto pressed(Key key) const noexcept -> bool;
        [[nodiscard]] auto released(Key key) const noexcept -> bool;

        // Pressed, or repeated by the OS while held, as typing repeats a
        // letter: for Backspace in a text field or stepping through a menu.
        [[nodiscard]] auto repeated(Key key) const noexcept -> bool;

        // Where the held keys point, of length 1 or 0: WASD or the arrows,
        // y down as on screen. A diagonal is as fast as a straight line, and
        // opposite keys cancel out.
        //
        //     player += input.direction() * speed * dt;
        [[nodiscard]] auto direction() const noexcept -> Vec2;

        // The same with four keys of your own.
        [[nodiscard]] auto direction(Key left, Key right, Key up, Key down) const noexcept -> Vec2;

        [[nodiscard]] auto down(MouseButton button) const noexcept -> bool;
        [[nodiscard]] auto pressed(MouseButton button) const noexcept -> bool;
        [[nodiscard]] auto released(MouseButton button) const noexcept -> bool;

        // In the window's screen coordinates, as the Canvas uses them: (0, 0)
        // at the top-left, y down. Canvas::to_world turns it into a world
        // point.
        [[nodiscard]] auto mouse() const noexcept -> Vec2;

        // How far the mouse moved since the poll before.
        [[nodiscard]] auto mouse_delta() const noexcept -> Vec2;

        // Scrolled since the poll before: y > 0 away from you, x sideways (on
        // a touchpad). One notch of a wheel is about 1.
        [[nodiscard]] auto wheel() const noexcept -> Vec2;

        // The characters typed since the poll before, in UTF-8, as the layout
        // and Shift make them: for text fields.
        [[nodiscard]] auto text() const noexcept -> std::string_view;

    private:
        friend class Window;

        Input() noexcept = default;
    };
}
