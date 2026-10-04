#include "tgx/input.h"

#include "input_internal.h"

#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {
    // Per key or mouse button: down now, and whether it went down or up since
    // the poll before. Both edges are kept, not worked out from down, so a
    // tap shorter than a frame still shows.
    enum : std::uint8_t {
        bit_down = 1,
        bit_pressed = 2,
        bit_released = 4,
        // Pressed or repeated by the OS since the poll before.
        bit_repeated = 8,
    };

    // The keys and the mouse of the one window, filled by GLFW callbacks
    // during a poll.
    struct InputState {
        std::array<std::uint8_t, tgx::key_count> keys{};
        std::array<std::uint8_t, tgx::mouse_button_count> buttons{};
        tgx::Vec2 mouse{};
        tgx::Vec2 mouse_delta{};
        tgx::Vec2 wheel{};
        std::string text;
    };

    InputState s_input;

    // GLFW's key codes to ours; keys we do not list are unknown.
    [[nodiscard]] auto to_key(int code) noexcept -> tgx::Key {
        using tgx::Key;

        const auto offset = [](Key first, int steps) noexcept {
            return static_cast<Key>(static_cast<int>(first) + steps);
        };
        if (code >= GLFW_KEY_A && code <= GLFW_KEY_Z) {
            return offset(Key::a, code - GLFW_KEY_A);
        }
        if (code >= GLFW_KEY_0 && code <= GLFW_KEY_9) {
            return offset(Key::digit0, code - GLFW_KEY_0);
        }
        if (code >= GLFW_KEY_F1 && code <= GLFW_KEY_F12) {
            return offset(Key::f1, code - GLFW_KEY_F1);
        }
        if (code >= GLFW_KEY_KP_0 && code <= GLFW_KEY_KP_9) {
            return offset(Key::keypad0, code - GLFW_KEY_KP_0);
        }

        switch (code) {
            case GLFW_KEY_SPACE: return Key::space;
            case GLFW_KEY_APOSTROPHE: return Key::apostrophe;
            case GLFW_KEY_COMMA: return Key::comma;
            case GLFW_KEY_MINUS: return Key::minus;
            case GLFW_KEY_PERIOD: return Key::period;
            case GLFW_KEY_SLASH: return Key::slash;
            case GLFW_KEY_SEMICOLON: return Key::semicolon;
            case GLFW_KEY_EQUAL: return Key::equal;
            case GLFW_KEY_LEFT_BRACKET: return Key::left_bracket;
            case GLFW_KEY_BACKSLASH: return Key::backslash;
            case GLFW_KEY_RIGHT_BRACKET: return Key::right_bracket;
            case GLFW_KEY_GRAVE_ACCENT: return Key::grave;
            case GLFW_KEY_ESCAPE: return Key::escape;
            case GLFW_KEY_ENTER: return Key::enter;
            case GLFW_KEY_TAB: return Key::tab;
            case GLFW_KEY_BACKSPACE: return Key::backspace;
            case GLFW_KEY_INSERT: return Key::insert;
            case GLFW_KEY_DELETE: return Key::del;
            case GLFW_KEY_RIGHT: return Key::right;
            case GLFW_KEY_LEFT: return Key::left;
            case GLFW_KEY_DOWN: return Key::down;
            case GLFW_KEY_UP: return Key::up;
            case GLFW_KEY_PAGE_UP: return Key::page_up;
            case GLFW_KEY_PAGE_DOWN: return Key::page_down;
            case GLFW_KEY_HOME: return Key::home;
            case GLFW_KEY_END: return Key::end;
            case GLFW_KEY_CAPS_LOCK: return Key::caps_lock;
            case GLFW_KEY_SCROLL_LOCK: return Key::scroll_lock;
            case GLFW_KEY_NUM_LOCK: return Key::num_lock;
            case GLFW_KEY_PRINT_SCREEN: return Key::print_screen;
            case GLFW_KEY_PAUSE: return Key::pause;
            case GLFW_KEY_KP_DECIMAL: return Key::keypad_decimal;
            case GLFW_KEY_KP_DIVIDE: return Key::keypad_divide;
            case GLFW_KEY_KP_MULTIPLY: return Key::keypad_multiply;
            case GLFW_KEY_KP_SUBTRACT: return Key::keypad_subtract;
            case GLFW_KEY_KP_ADD: return Key::keypad_add;
            case GLFW_KEY_KP_ENTER: return Key::keypad_enter;
            case GLFW_KEY_KP_EQUAL: return Key::keypad_equal;
            case GLFW_KEY_LEFT_SHIFT: return Key::left_shift;
            case GLFW_KEY_LEFT_CONTROL: return Key::left_control;
            case GLFW_KEY_LEFT_ALT: return Key::left_alt;
            case GLFW_KEY_LEFT_SUPER: return Key::left_super;
            case GLFW_KEY_RIGHT_SHIFT: return Key::right_shift;
            case GLFW_KEY_RIGHT_CONTROL: return Key::right_control;
            case GLFW_KEY_RIGHT_ALT: return Key::right_alt;
            case GLFW_KEY_RIGHT_SUPER: return Key::right_super;
            case GLFW_KEY_MENU: return Key::menu;
            default: return Key::unknown;
        }
    }

    // A repeat is neither a press nor a change of down: a held key went down
    // once. Only repeated() sees it.
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
        if (const tgx::Key key = to_key(code); key != tgx::Key::unknown) {
            set_edge(s_input.keys[static_cast<std::size_t>(key)], action);
        }
    }

    auto on_mouse_button(GLFWwindow *, int button, int action, int) noexcept -> void {
        // GLFW numbers them left, right, middle, then the side ones, as we do.
        if (button >= 0 && button < tgx::mouse_button_count) {
            set_edge(s_input.buttons[static_cast<std::size_t>(button)], action);
        }
    }

    auto on_cursor(GLFWwindow *, double x, double y) noexcept -> void {
        const tgx::Vec2 mouse{static_cast<float>(x), static_cast<float>(y)};
        s_input.mouse_delta += mouse - s_input.mouse;
        s_input.mouse = mouse;
    }

    auto on_scroll(GLFWwindow *, double x, double y) noexcept -> void {
        s_input.wheel += {static_cast<float>(x), static_cast<float>(y)};
    }

    // Appends the code point as UTF-8.
    auto on_char(GLFWwindow *, unsigned int c) noexcept -> void {
        std::string &text = s_input.text;
        const auto byte = [](unsigned int b) noexcept { return static_cast<char>(b); };
        if (c < 0x80) {
            text += byte(c);
        } else if (c < 0x800) {
            text += byte(0xC0 | (c >> 6));
            text += byte(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            text += byte(0xE0 | (c >> 12));
            text += byte(0x80 | ((c >> 6) & 0x3F));
            text += byte(0x80 | (c & 0x3F));
        } else {
            text += byte(0xF0 | (c >> 18));
            text += byte(0x80 | ((c >> 12) & 0x3F));
            text += byte(0x80 | ((c >> 6) & 0x3F));
            text += byte(0x80 | (c & 0x3F));
        }
    }
}

namespace tgx {
    auto detail::attach_input(GLFWwindow *handle) noexcept -> void {
        s_input = {};

        // Where the mouse already is, so the first move is not a jump from
        // the corner.
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(handle, &x, &y);
        s_input.mouse = {static_cast<float>(x), static_cast<float>(y)};

        glfwSetKeyCallback(handle, on_key);
        glfwSetMouseButtonCallback(handle, on_mouse_button);
        glfwSetCursorPosCallback(handle, on_cursor);
        glfwSetScrollCallback(handle, on_scroll);
        glfwSetCharCallback(handle, on_char);
    }

    auto detail::begin_input_frame() noexcept -> void {
        // What happened since the poll before is forgotten; what is down
        // stays down.
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

    auto Input::down(Key key) const noexcept -> bool {
        return (s_input.keys[static_cast<std::size_t>(key)] & bit_down) != 0;
    }

    auto Input::pressed(Key key) const noexcept -> bool {
        return (s_input.keys[static_cast<std::size_t>(key)] & bit_pressed) != 0;
    }

    auto Input::released(Key key) const noexcept -> bool {
        return (s_input.keys[static_cast<std::size_t>(key)] & bit_released) != 0;
    }

    auto Input::repeated(Key key) const noexcept -> bool {
        return (s_input.keys[static_cast<std::size_t>(key)] & bit_repeated) != 0;
    }

    auto Input::down(MouseButton button) const noexcept -> bool {
        return (s_input.buttons[static_cast<std::size_t>(button)] & bit_down) != 0;
    }

    auto Input::pressed(MouseButton button) const noexcept -> bool {
        return (s_input.buttons[static_cast<std::size_t>(button)] & bit_pressed) != 0;
    }

    auto Input::released(MouseButton button) const noexcept -> bool {
        return (s_input.buttons[static_cast<std::size_t>(button)] & bit_released) != 0;
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
}
