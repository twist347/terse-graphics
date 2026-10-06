// The picture follows the order of the calls, though the Canvas's shapes wait
// in a batch: they are drawn before a clear or a draw of the Device's own.

#include "gpu.h"

#include <doctest/doctest.h>

TEST_CASE("shapes waiting are drawn before a Device clear") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.rect({0, 0, 64, 64}, tgx::colors::red);
    tgx_test::app().device().clear({.target = &target, .color = tgx::colors::blue});

    CHECK(target.read().at(10, 10) == tgx::colors::blue);
}

TEST_CASE("shapes waiting are drawn before a Device draw, and later ones after it") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const auto fill = tgx_test::Fill::create();

    canvas.rect({0, 0, 64, 64}, tgx::colors::red);
    fill.draw({.target = &target});
    canvas.rect({0, 0, 20, 20}, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image.at(5, 5) == tgx::colors::white);
    CHECK(image.at(40, 40) == tgx::colors::green);
}

TEST_CASE("Canvas::clear covers what came before it, all of it") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.rect({0, 0, 64, 64}, tgx::colors::red);
    canvas.clear(tgx::colors::green);
    canvas.rect({0, 0, 20, 20}, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image.at(5, 5) == tgx::colors::white);
    CHECK(image.at(60, 60) == tgx::colors::green);
}

TEST_CASE("two canvases share the batch, each with its own state, in call order") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    tgx::Canvas additive = canvas;
    additive.set_blend(tgx::Blend::additive);

    canvas.clear(tgx::colors::black);
    canvas.rect({0, 0, 64, 64}, tgx::colors::red);
    additive.rect({0, 0, 64, 64}, tgx::colors::green);
    canvas.rect({0, 0, 20, 20}, tgx::colors::blue);

    const tgx::Image image = target.read();
    CHECK(image.at(60, 60) == tgx::colors::yellow);
    CHECK(image.at(5, 5) == tgx::colors::blue);
}

TEST_CASE("a copy with a camera and the original drawn in between keep the call order") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    tgx::Canvas world = canvas;
    world.set_camera({.offset = {10, 10}, .zoom = 2.f});

    canvas.clear(tgx::colors::black);
    world.rect({0, 0, 10, 10}, tgx::colors::white);   // 10 to 30 on the target
    canvas.rect({15, 15, 5, 5}, tgx::colors::red);    // on top of it
    world.rect({20, 20, 5, 5}, tgx::colors::blue);    // 50 to 60, after it again

    const tgx::Image image = target.read();
    CHECK(image.at(12, 12) == tgx::colors::white);
    CHECK(image.at(17, 17) == tgx::colors::red);
    CHECK(image.at(55, 55) == tgx::colors::blue);
}

TEST_CASE("more shapes than one batch holds, then one on top") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    for (int i = 0; i < 10000; ++i) {
        canvas.rect({0, 0, 1, 1}, tgx::colors::red);
    }
    for (int i = 0; i < 3000; ++i) {
        canvas.circle({32, 32}, 30, tgx::colors::red);
    }
    canvas.rect({0, 0, 20, 20}, tgx::colors::white);
    canvas.circle({40, 40}, 10, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image.at(5, 5) == tgx::colors::white);
    CHECK(image.at(40, 40) == tgx::colors::white);
    CHECK(image.at(32, 10) == tgx::colors::red);
}
