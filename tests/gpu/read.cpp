// The window itself: Device::read, a Device draw's viewport and the canvas
// on it, in the window's screen coordinates; its settings.

#include "gpu.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

namespace {
    // A point in the window's screen coordinates, in its framebuffer's
    // pixels: more of them on a display with a scale.
    [[nodiscard]] auto pixel(tgx::Vec2 point) -> tgx::Vec2 {
        const tgx::Window &window = tgx_test::app().window();
        const tgx::Size pixels = window.framebuffer_size();
        const tgx::Size size = window.size();
        return {
            point.x * static_cast<float>(pixels.width) / static_cast<float>(size.width),
            point.y * static_cast<float>(pixels.height) / static_cast<float>(size.height),
        };
    }

    [[nodiscard]] auto at(const tgx::Image &image, tgx::Vec2 point) -> tgx::Color {
        const tgx::Vec2 p = pixel(point);
        return image[static_cast<int>(p.x), static_cast<int>(p.y)];
    }
}

TEST_CASE("Device::read: the window's framebuffer, opaque, rows top to bottom") {
    auto &app = tgx_test::app();
    tgx::Canvas screen = tgx::Canvas::create();
    CHECK(screen.size() == app.window().size());

    screen.clear(tgx::colors::black);
    screen.rect({0, 0, 20, 20}, tgx::colors::red);
    screen.rect({20, 0, 20, 20}, tgx::colors::white.with_alpha(128));

    const tgx::Image image = app.device().read();
    CHECK(image.size() == app.window().framebuffer_size());
    CHECK(at(image, {5, 5}) == tgx::colors::red);
    CHECK(at(image, {5, 30}) == tgx::colors::black);
    CHECK(tgx_test::near(at(image, {30, 5}), tgx::Color{128, 128, 128, 255}, 1));
}

TEST_CASE("a Device draw with a viewport counts it from the window's top-left") {
    auto &app = tgx_test::app();
    const tgx::Size pixels = app.window().framebuffer_size();
    const tgx::Size size = app.window().size();
    const auto cover = tgx_test::Cover::create();

    app.device().clear({.color = tgx::colors::black});
    cover.draw({.viewport = tgx::gl::Viewport{0, 0, pixels.width / 2, pixels.height / 2}});

    const tgx::Image image = app.device().read();
    const float w = static_cast<float>(size.width);
    const float h = static_cast<float>(size.height);
    CHECK(at(image, {5, 5}) == tgx::colors::green);
    CHECK(at(image, {w - 5, 5}) == tgx::colors::black);
    CHECK(at(image, {5, h - 5}) == tgx::colors::black);
}

TEST_CASE("a screenshot saved and loaded back is the same picture") {
    auto &app = tgx_test::app();
    tgx::Canvas screen = tgx::Canvas::create();
    screen.clear(tgx::colors::black);
    screen.rect({0, 0, 20, 20}, tgx::colors::green);
    const tgx::Image shot = app.device().read();

    const tgx_test::TempFile file{"screenshot.png"};
    REQUIRE(shot.save(file.path).has_value());
    const auto back = tgx::Image::load(file.path);
    REQUIRE(back.has_value());
    CHECK(back->size() == shot.size());
    CHECK(at(*back, {5, 5}) == tgx::colors::green);
    CHECK(at(*back, {30, 30}) == tgx::colors::black);
}

TEST_CASE("Window::vsync reads back as set") {
    tgx::Window &window = tgx_test::app().window();
    CHECK_FALSE(window.vsync());
    window.set_vsync(true);
    CHECK(window.vsync());
    window.set_vsync(false);
}
