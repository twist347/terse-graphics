// Viewports, sizes and cameras: where on what it draws into a shape lands,
// and the way back from a point there to the world.

#include "gpu.h"

#include "support.h"

#include <doctest/doctest.h>

TEST_CASE("a Device draw with a viewport counts it from the top-left") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    const auto cover = tgx_test::Cover::create();
    auto &device = tgx_test::app().device();

    device.clear({.target = &target, .color = tgx::colors::black});
    cover.draw({.target = &target, .viewport = tgx::gl::Viewport{0, 0, 32, 32}});
    tgx::Image image = target.read();
    CHECK(image[5, 5] == tgx::colors::green);
    CHECK(image[60, 5] == tgx::colors::black);
    CHECK(image[5, 60] == tgx::colors::black);

    device.clear({.target = &target, .color = tgx::colors::black});
    cover.draw({.target = &target, .viewport = tgx::gl::Viewport{54, 54, 10, 10}});
    image = target.read();
    CHECK(image[59, 59] == tgx::colors::green);
    CHECK(image[59, 5] == tgx::colors::black);
}

TEST_CASE("a Device draw with a viewport leaves the canvas whole") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const auto cover = tgx_test::Cover::create();

    canvas.rect({0, 0, 64, 64}, tgx::colors::red);
    cover.draw({.target = &target, .viewport = tgx::gl::Viewport{0, 0, 32, 32}});
    canvas.rect({0, 0, 20, 20}, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image[5, 5] == tgx::colors::white);
    CHECK(image[25, 25] == tgx::colors::green);
    CHECK(image[60, 5] == tgx::colors::red);
}

TEST_CASE("a canvas with a viewport: a minimap") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    tgx::Canvas minimap = canvas;
    minimap.set_viewport({32, 0, 32, 32});   // the top-right quarter
    CHECK(minimap.size() == tgx::Size{32, 32});

    canvas.clear(tgx::colors::black);
    canvas.rect({0, 0, 10, 10}, tgx::colors::red);
    minimap.rect({0, 0, 32, 32}, tgx::colors::blue);   // all of its own
    minimap.rect({0, 0, 4, 4}, tgx::colors::yellow);   // its own top-left
    canvas.rect({20, 0, 4, 4}, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image[60, 10] == tgx::colors::blue);
    CHECK(image[34, 2] == tgx::colors::yellow);
    CHECK(image[60, 60] == tgx::colors::black);
    CHECK(image[5, 5] == tgx::colors::red);
    CHECK(image[21, 2] == tgx::colors::white);
}

TEST_CASE("a canvas of a size of its own is stretched over its viewport") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    tgx::Canvas pixels = canvas;
    pixels.set_viewport({32, 0, 32, 32});
    pixels.set_size({4, 2});

    canvas.clear(tgx::colors::black);
    pixels.rect({2, 1, 2, 1}, tgx::colors::green);   // its bottom-right quarter

    const tgx::Image image = target.read();
    CHECK(image[60, 28] == tgx::colors::green);
    CHECK(image[44, 28] == tgx::colors::black);
    CHECK(image[60, 12] == tgx::colors::black);
}

TEST_CASE("set_size, then back to following what it draws into") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.set_size({32, 32});
    canvas.clear(tgx::colors::black);
    canvas.rect({16, 16, 16, 16}, tgx::colors::yellow);
    CHECK(target.read()[60, 60] == tgx::colors::yellow);

    canvas.set_size({});
    CHECK(canvas.size() == tgx::Size{64, 64});
}

TEST_CASE("to_world and to_screen go through the viewport, the size and the camera") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    tgx::Canvas minimap = canvas;
    minimap.set_viewport({32, 0, 32, 32});

    CHECK(tgx_test::near(minimap.to_world({42, 5}), {10, 5}));

    tgx::Canvas pixels = minimap;
    pixels.set_size({4, 2});
    CHECK(tgx_test::near(pixels.to_world({48, 16}), {2, 1}));
    CHECK(tgx_test::near(pixels.to_screen({2, 1}), {48, 16}));

    tgx::Canvas camera = minimap;
    camera.set_camera({.target = {50, 50}, .offset = {10, 10}, .zoom = 2.f});
    CHECK(tgx_test::near(camera.to_world({42, 10}), {50, 50}));
    CHECK(tgx_test::near(camera.to_world(camera.to_screen({7, -3})), {7, -3}));

    CHECK(tgx_test::near(canvas.to_world({33, 44}), {33, 44}));
}

TEST_CASE("a camera holds for the shapes after it, until reset") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.set_camera({.offset = {10, 10}, .zoom = 2.f});
    canvas.rect({0, 0, 5, 5}, tgx::colors::white);   // 10 to 20 on the target
    canvas.set_camera({});
    canvas.rect({0, 0, 5, 5}, tgx::colors::yellow);

    const tgx::Image image = target.read();
    CHECK(image[15, 15] == tgx::colors::white);
    CHECK(image[2, 2] == tgx::colors::yellow);
    CHECK(image[25, 25] == tgx::colors::black);
}
