// The installed GLFW callbacks update the same Input the App exposes.
#define GLFW_INCLUDE_NONE
#include "gpu.hpp"

#include <doctest/doctest.h>
#include <GLFW/glfw3.h>

namespace {
    struct InputCallbacks {
        GLFWwindow *handle{tgx_test::app().window().native_handle()};
        GLFWkeyfun key{};
        GLFWmousebuttonfun button{};
        GLFWcursorposfun cursor{};
        GLFWscrollfun scroll{};
        GLFWcharfun character{};

        InputCallbacks() {
            key = glfwSetKeyCallback(handle, nullptr);
            glfwSetKeyCallback(handle, key);
            button = glfwSetMouseButtonCallback(handle, nullptr);
            glfwSetMouseButtonCallback(handle, button);
            cursor = glfwSetCursorPosCallback(handle, nullptr);
            glfwSetCursorPosCallback(handle, cursor);
            scroll = glfwSetScrollCallback(handle, nullptr);
            glfwSetScrollCallback(handle, scroll);
            character = glfwSetCharCallback(handle, nullptr);
            glfwSetCharCallback(handle, character);
        }

        ~InputCallbacks() {
            if (key) {
                key(handle, GLFW_KEY_F12, 0, GLFW_RELEASE, 0);
            }
            if (button) {
                button(handle, GLFW_MOUSE_BUTTON_4, GLFW_RELEASE, 0);
            }
            // Restore the real cursor position after the synthetic moves.
            if (cursor) {
                double x = 0.0, y = 0.0;
                glfwGetCursorPos(handle, &x, &y);
                cursor(handle, x, y);
            }
            tgx_test::app().window().poll_events();
        }
    };
}

TEST_CASE("GLFW callbacks feed Input and a poll clears only its events") {
    auto &app = tgx_test::app();
    auto &window = app.window();
    window.poll_events();
    const InputCallbacks callbacks;
    REQUIRE(callbacks.key);
    REQUIRE(callbacks.button);
    REQUIRE(callbacks.cursor);
    REQUIRE(callbacks.scroll);
    REQUIRE(callbacks.character);
    const auto &input = app.input();
    CHECK(&input == &window.input());

    callbacks.key(callbacks.handle, GLFW_KEY_F12, 0, GLFW_PRESS, 0);
    callbacks.button(callbacks.handle, GLFW_MOUSE_BUTTON_4, GLFW_PRESS, 0);
    CHECK(input.down(tgx::Key::f12));
    CHECK(input.pressed(tgx::Key::f12));
    CHECK(input.repeated(tgx::Key::f12));
    CHECK(input.down(tgx::MouseButton::x1));
    CHECK(input.pressed(tgx::MouseButton::x1));

    window.poll_events();
    CHECK(input.down(tgx::Key::f12));
    CHECK_FALSE(input.pressed(tgx::Key::f12));
    CHECK_FALSE(input.repeated(tgx::Key::f12));
    CHECK(input.down(tgx::MouseButton::x1));
    CHECK_FALSE(input.pressed(tgx::MouseButton::x1));

    callbacks.key(callbacks.handle, GLFW_KEY_F12, 0, GLFW_REPEAT, 0);
    CHECK(input.repeated(tgx::Key::f12));
    CHECK_FALSE(input.pressed(tgx::Key::f12));
    callbacks.key(callbacks.handle, GLFW_KEY_UNKNOWN, 0, GLFW_PRESS, 0);
    callbacks.key(callbacks.handle, GLFW_KEY_F13, 0, GLFW_PRESS, 0);
    CHECK_FALSE(input.down(tgx::Key::unknown));

    callbacks.cursor(callbacks.handle, 11.25, 23.5);
    CHECK(input.mouse() == tgx::Vec2{11.25f, 23.5f});
    callbacks.scroll(callbacks.handle, 0.25, 1.0);
    callbacks.scroll(callbacks.handle, 0.75, -0.5);
    CHECK(input.wheel() == tgx::Vec2{1.f, 0.5f});
    callbacks.character(callbacks.handle, U'a');
    callbacks.character(callbacks.handle, U'\u20ac');
    CHECK(input.text() == "a\xe2\x82\xac");

    callbacks.key(callbacks.handle, GLFW_KEY_F12, 0, GLFW_RELEASE, 0);
    callbacks.button(callbacks.handle, GLFW_MOUSE_BUTTON_4, GLFW_RELEASE, 0);
    CHECK_FALSE(input.down(tgx::Key::f12));
    CHECK(input.released(tgx::Key::f12));
    CHECK_FALSE(input.down(tgx::MouseButton::x1));
    CHECK(input.released(tgx::MouseButton::x1));

    window.poll_events();
    CHECK_FALSE(input.released(tgx::Key::f12));
    CHECK_FALSE(input.repeated(tgx::Key::f12));
    CHECK_FALSE(input.released(tgx::MouseButton::x1));
    CHECK(input.wheel() == tgx::Vec2{});
    CHECK(input.text().empty());
}
