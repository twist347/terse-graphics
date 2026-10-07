// Render targets: drawn into, read back, drawn themselves, and going while
// shapes for them wait.

#include "gpu.h"

#include <doctest/doctest.h>

#include <initializer_list>
#include <utility>

namespace {
    // Red top-left quarter on blue.
    auto draw_quarter(tgx::Canvas &canvas) -> void {
        canvas.clear(tgx::colors::blue);
        canvas.rect({0, 0, 32, 16}, tgx::colors::red);
    }
}

TEST_CASE("a canvas on a target spans its pixels, read back rows top to bottom") {
    const tgx::RenderTarget target = tgx_test::target({64, 32});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    CHECK(canvas.size() == tgx::Size{64, 32});

    draw_quarter(canvas);

    const tgx::Image image = target.read();
    CHECK(image.size() == tgx::Size{64, 32});
    CHECK(image[0, 0] == tgx::colors::red);
    CHECK(image[31, 15] == tgx::colors::red);
    CHECK(image[32, 0] == tgx::colors::blue);
    CHECK(image[0, 16] == tgx::colors::blue);
    CHECK(image[63, 31] == tgx::colors::blue);
}

TEST_CASE("a target drawn as a sprite: the right way up, mirrored, a part of it") {
    const tgx::RenderTarget target = tgx_test::target({64, 32});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    draw_quarter(canvas);
    const tgx::RenderTarget out = tgx_test::target({64, 64});
    tgx::Canvas screen = tgx_test::canvas_on(out);

    screen.clear(tgx::colors::black);
    screen.sprite(target.texture(), {.size = {64, 32}});
    tgx::Image image = out.read();
    CHECK(image[2, 2] == tgx::colors::red);
    CHECK(image[2, 28] == tgx::colors::blue);
    CHECK(image[60, 2] == tgx::colors::blue);
    CHECK(image[32, 48] == tgx::colors::black);

    screen.clear(tgx::colors::black);
    screen.sprite(target.texture(), {.size = {64, 32}, .src = {0, 0, 64, -32}});
    image = out.read();
    CHECK(image[2, 28] == tgx::colors::red);
    CHECK(image[2, 2] == tgx::colors::blue);

    screen.clear(tgx::colors::black);
    screen.sprite(target.texture(), {.size = {32, 32}, .src = {0, 0, 32, 16}});
    image = out.read();
    CHECK(image[16, 30] == tgx::colors::red);
    CHECK(image[40, 16] == tgx::colors::black);
}

TEST_CASE("a canvas with a viewport on a target, and to_world in its pixels") {
    const tgx::RenderTarget target = tgx_test::target({64, 32});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    draw_quarter(canvas);

    tgx::Canvas quarter = canvas;
    quarter.set_viewport({32, 16, 32, 16});
    CHECK(quarter.size() == tgx::Size{32, 16});
    quarter.rect({0, 0, 32, 16}, tgx::colors::yellow);

    const tgx::Image image = target.read();
    CHECK(image[40, 20] == tgx::colors::yellow);
    CHECK(image[0, 0] == tgx::colors::red);
    CHECK(image[40, 4] == tgx::colors::blue);

    tgx::Canvas small = canvas;
    small.set_size({16, 8});
    CHECK(small.to_world({32, 16}) == tgx::Vec2{8, 4});
}

TEST_CASE("draws into two targets keep the call order") {
    const tgx::RenderTarget target = tgx_test::target({64, 32});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const tgx::RenderTarget out = tgx_test::target({64, 64});
    tgx::Canvas screen = tgx_test::canvas_on(out);

    screen.clear(tgx::colors::black);
    screen.rect({0, 0, 64, 64}, tgx::colors::white);
    canvas.clear(tgx::colors::magenta);
    canvas.rect({0, 0, 64, 32}, tgx::colors::cyan);
    screen.sprite(target.texture(), {.size = {64, 32}});
    screen.rect({40, 40, 10, 10}, tgx::colors::red);

    const tgx::Image image = out.read();
    CHECK(image[10, 10] == tgx::colors::cyan);
    CHECK(image[10, 50] == tgx::colors::white);
    CHECK(image[45, 45] == tgx::colors::red);
}

TEST_CASE("a target destroyed while shapes for it wait") {
    {
        const tgx::RenderTarget doomed = tgx_test::target({16, 16});
        tgx::Canvas canvas = tgx_test::canvas_on(doomed);
        canvas.rect({0, 0, 8, 8}, tgx::colors::red);
    }
    const tgx::RenderTarget after = tgx_test::target({16, 16});
    tgx::Canvas canvas = tgx_test::canvas_on(after);
    canvas.clear(tgx::colors::black);
    canvas.rect({0, 0, 4, 4}, tgx::colors::green);

    const tgx::Image image = after.read();
    CHECK(image[1, 1] == tgx::colors::green);
    CHECK(image[10, 10] == tgx::colors::black);
}

TEST_CASE("a target moved and move-assigned while shapes for it wait") {
    tgx::RenderTarget a = tgx_test::target({16, 16});
    tgx::Canvas canvas = tgx_test::canvas_on(a);
    canvas.clear(tgx::colors::black);
    canvas.rect({0, 0, 16, 16}, tgx::colors::red);

    tgx::RenderTarget moved = std::move(a);
    CHECK(moved.read()[8, 8] == tgx::colors::red);

    // The canvas still draws into what moved holds, which goes with blue
    // waiting for it.
    canvas.rect({0, 0, 16, 16}, tgx::colors::blue);
    moved = tgx_test::target({8, 8});
    tgx::Canvas other = tgx_test::canvas_on(moved);
    other.clear(tgx::colors::green);
    CHECK(moved.read()[4, 4] == tgx::colors::green);
}

TEST_CASE("a target with a depth buffer takes a draw with a depth test") {
    const tgx::RenderTarget target = tgx_test::target({32, 32}, true);
    CHECK(target.has_depth());
    const auto cover = tgx_test::Cover::create();

    tgx_test::app().device().clear({.target = &target, .color = tgx::colors::black, .depth = 1.f});
    cover.draw({.target = &target, .state = {.depth = tgx::gl::Depth::less}});
    CHECK(target.read()[5, 5] == tgx::colors::green);
}

TEST_CASE("a target larger than the driver draws into") {
    const auto huge = tgx::RenderTarget::create({100000, 16});
    REQUIRE_FALSE(huge.has_value());
    CHECK(huge.error() == tgx::Error::unsupported);
}

TEST_CASE("see-through parts of a target are premultiplied") {
    const tgx::RenderTarget layer = tgx_test::target({8, 8});
    tgx::Canvas canvas = tgx_test::canvas_on(layer);
    canvas.clear(tgx::colors::transparent);
    canvas.rect({0, 0, 8, 8}, tgx::colors::white.with_alpha(128));
    CHECK(layer.read()[4, 4] == tgx::Color{128, 128, 128, 128});

    const tgx::RenderTarget out = tgx_test::target({8, 8});
    for (const tgx::Blend blend: {tgx::Blend::alpha, tgx::Blend::premultiplied}) {
        CAPTURE(static_cast<int>(blend));
        tgx::Canvas screen = tgx_test::canvas_on(out);
        screen.clear(tgx::colors::black);
        screen.set_blend(blend);
        screen.sprite(layer.texture(), {.size = {8, 8}});
        // Alpha applies the alpha a second time: a quarter gray, not half.
        const tgx::Color expected = blend == tgx::Blend::alpha ? tgx::Color{64, 64, 64, 255} : tgx::Color{128, 128, 128, 255};
        CHECK(out.read()[2, 2] == expected);
    }
}

TEST_CASE("a new target starts transparent, its depth at the far end") {
    const tgx::RenderTarget target = tgx_test::target({32, 32}, true);
    CHECK(target.read()[5, 5] == tgx::colors::transparent);

    // No clear first: the depth test passes against what create left.
    const auto cover = tgx_test::Cover::create();
    cover.draw({.target = &target, .state = {.depth = tgx::gl::Depth::less}});
    CHECK(target.read()[5, 5] == tgx::colors::green);
}

TEST_CASE("a clear of a target premultiplies its color, as drawing does") {
    const tgx::RenderTarget layer = tgx_test::target({8, 8});
    tgx_test::canvas_on(layer).clear(tgx::colors::white.with_alpha(128));
    CHECK(layer.read()[4, 4] == tgx::Color{128, 128, 128, 128});

    tgx_test::app().device().clear({.target = &layer, .color = tgx::colors::white.with_alpha(128)});
    CHECK(layer.read()[4, 4] == tgx::Color{128, 128, 128, 128});
}
