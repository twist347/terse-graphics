// GL and GLFW called directly, through the headers tgx ships: glad loaded by
// the Device, the window from native_handle(). Only public includes.

#include "gpu.hpp"

#include "support.hpp"

#include <doctest/doctest.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

TEST_CASE("raw GL: glad is loaded, and a scissor set around a Device clear clips it") {
    auto &app = tgx_test::app();
    CHECK(glGetString(GL_VERSION));

    const tgx::RenderTarget target = tgx_test::target({4, 4});
    app.device().clear({.target = &target, .color = tgx::colors::green});

    app.device().flush();
    glEnable(GL_SCISSOR_TEST);
    // The bottom-left quarter: GL counts from the bottom-left.
    glScissor(0, 0, 2, 2);
    app.device().clear({.target = &target, .color = tgx::colors::red});
    glDisable(GL_SCISSOR_TEST);

    const tgx::Image image = target.read();
    CHECK(image[0, 3] == tgx::colors::red);
    CHECK(image[1, 2] == tgx::colors::red);
    CHECK(image[2, 1] == tgx::colors::green);
    CHECK(image[3, 3] == tgx::colors::green);
    CHECK(image[0, 0] == tgx::colors::green);
}

TEST_CASE("raw GLFW: native_handle() is the window") {
    const tgx::Window &window = tgx_test::app().window();
    GLFWwindow *handle = window.native_handle();
    REQUIRE(handle);

    tgx::Size size{};
    glfwGetWindowSize(handle, &size.width, &size.height);
    CHECK(size == window.size());
    CHECK(glfwGetWindowAttrib(handle, GLFW_CONTEXT_VERSION_MAJOR) >= 3);
}
