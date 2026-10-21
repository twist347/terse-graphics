// Canvas::set_clip: shapes cut to a rect, which ignores the camera and goes
// through the canvas's size and viewport.

#include "gpu.hpp"

#include <doctest/doctest.h>
#include <glad/gl.h>

TEST_CASE("a clip cuts shapes to its rect, and none draws them whole") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.set_clip(tgx::Rect{16, 8, 32, 24});
    canvas.rect({0, 0, 64, 64}, tgx::colors::red);
    canvas.set_clip({});
    canvas.rect({0, 56, 64, 8}, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image[16, 8] == tgx::colors::red);
    CHECK(image[47, 31] == tgx::colors::red);
    CHECK(image[15, 20] == tgx::colors::black);
    CHECK(image[48, 20] == tgx::colors::black);
    CHECK(image[30, 7] == tgx::colors::black);
    CHECK(image[30, 32] == tgx::colors::black);
    CHECK(image[0, 60] == tgx::colors::white);
    CHECK(image[63, 60] == tgx::colors::white);
}

TEST_CASE("an empty clip shows nothing") {
    const tgx::RenderTarget target = tgx_test::target({16, 16});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.set_clip(tgx::Rect{4, 4, 0, 8});   // a bar filled 0 %
    canvas.rect({0, 0, 16, 16}, tgx::colors::red);

    const tgx::Image image = target.read();
    CHECK(image[4, 8] == tgx::colors::black);
    CHECK(image[8, 8] == tgx::colors::black);
}

TEST_CASE("a clip ignores the camera") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    canvas.set_camera({.target = {100, 100}, .offset = {32, 32}, .rotation = 0.5f, .zoom = 3});

    canvas.clear(tgx::colors::black);
    canvas.set_clip(tgx::Rect{0, 0, 32, 32});
    canvas.circle({100, 100}, 100, tgx::colors::red);   // covers all of it

    const tgx::Image image = target.read();
    CHECK(image[0, 0] == tgx::colors::red);
    CHECK(image[31, 31] == tgx::colors::red);
    CHECK(image[32, 5] == tgx::colors::black);
    CHECK(image[5, 32] == tgx::colors::black);
}

TEST_CASE("a clip is in the canvas's own units, within its viewport") {
    // 4x2 in a 32x32 viewport: 32x16 at (32, 8), 8 pixels a unit.
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    tgx::Canvas pixels = canvas;
    pixels.set_viewport({32, 0, 32, 32});
    pixels.set_size({4, 2});

    canvas.clear(tgx::colors::black);
    pixels.set_clip(tgx::Rect{1, 0, 10, 1});   // past its right edge
    pixels.rect({0, 0, 4, 2}, tgx::colors::green);

    const tgx::Image image = target.read();
    CHECK(image[40, 8] == tgx::colors::green);
    CHECK(image[63, 15] == tgx::colors::green);
    CHECK(image[39, 10] == tgx::colors::black);
    CHECK(image[50, 16] == tgx::colors::black);
}

TEST_CASE("clear ignores the clip") {
    const tgx::RenderTarget target = tgx_test::target({16, 16});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.set_clip(tgx::Rect{0, 0, 4, 4});
    canvas.rect({0, 0, 16, 16}, tgx::colors::red);
    canvas.clear(tgx::colors::blue);

    const tgx::Image image = target.read();
    CHECK(image[1, 1] == tgx::colors::blue);
    CHECK(image[12, 12] == tgx::colors::blue);
}

TEST_CASE("a change of clip starts a new batch, and GL is left without a scissor") {
    auto &device = tgx_test::app().device();
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    const auto frame = [&] {
        canvas.set_clip(tgx::Rect{0, 0, 4, 4});
        canvas.rect({0, 0, 8, 8}, tgx::colors::red);
        canvas.rect({0, 0, 8, 8}, tgx::colors::red);
        canvas.set_clip(tgx::Rect{4, 4, 4, 4});
        canvas.rect({0, 0, 8, 8}, tgx::colors::blue);
        canvas.set_clip({});
        canvas.rect({0, 0, 1, 1}, tgx::colors::white);
        device.flush();
        CHECK_FALSE(glIsEnabled(GL_SCISSOR_TEST));
        device.present();
    };
    frame();
    frame();

    CHECK(device.stats().canvas_draws == 3);
}
