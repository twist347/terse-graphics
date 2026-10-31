// GLFW key codes translate without a window or a GL header.
#define GLFW_INCLUDE_NONE
#include "platform/glfw_keys.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>

TEST_CASE("GLFW key ranges and unsupported keys") {
    using tgx::Key;
    using tgx::detail::to_key;
    CHECK(to_key(GLFW_KEY_A) == Key::a);
    CHECK(to_key(GLFW_KEY_Z) == Key::z);
    CHECK(to_key(GLFW_KEY_0) == Key::digit0);
    CHECK(to_key(GLFW_KEY_9) == Key::digit9);
    CHECK(to_key(GLFW_KEY_F1) == Key::f1);
    CHECK(to_key(GLFW_KEY_F12) == Key::f12);
    CHECK(to_key(GLFW_KEY_KP_0) == Key::keypad0);
    CHECK(to_key(GLFW_KEY_KP_9) == Key::keypad9);
    CHECK(to_key(GLFW_KEY_LEFT_SHIFT) == Key::left_shift);
    CHECK(to_key(GLFW_KEY_RIGHT_SHIFT) == Key::right_shift);
    CHECK(to_key(GLFW_KEY_MENU) == Key::menu);
    CHECK(to_key(GLFW_KEY_UNKNOWN) == Key::unknown);
    CHECK(to_key(GLFW_KEY_WORLD_1) == Key::unknown);
    CHECK(to_key(GLFW_KEY_WORLD_2) == Key::unknown);
    CHECK(to_key(GLFW_KEY_F13) == Key::unknown);
    CHECK(to_key(GLFW_KEY_LAST + 1) == Key::unknown);
}

TEST_CASE("each tgx key has exactly one GLFW key code") {
    std::array<int, tgx::key_count> occurrences{};
    for (int code = 0; code <= GLFW_KEY_LAST; ++code) {
        const tgx::Key key = tgx::detail::to_key(code);
        if (key != tgx::Key::unknown) {
            ++occurrences[static_cast<std::size_t>(key)];
        }
    }
    for (std::size_t key = 1; key < occurrences.size(); ++key) {
        CAPTURE(key);
        CHECK(occurrences[key] == 1);
    }
}
