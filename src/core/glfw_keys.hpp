#pragma once

#include "tgx/core/input.hpp"

#include <GLFW/glfw3.h>

// GLFW's key codes as tgx Keys.
namespace tgx::detail {
    // Keys not listed here are unknown.
    [[nodiscard]] inline auto to_key(int code) noexcept -> Key {
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
}
