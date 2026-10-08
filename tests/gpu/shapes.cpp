// The Canvas's shapes: edges, joints, gradients and blending, checked where
// they show.

#include "gpu.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>

TEST_CASE("a circle's edge") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.circle({32, 32}, 20, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image[32, 32] == tgx::colors::white);
    CHECK(image[50, 32] == tgx::colors::white);
    CHECK(image[54, 32] == tgx::colors::black);
}

TEST_CASE("a circle stretched with its canvas stays round") {
    // 32x32 over 256x256 pixels: a radius of 5 is 40 pixels, and has as many
    // segments as 40 pixels need.
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    canvas.set_size({32, 32});

    canvas.clear(tgx::colors::black);
    canvas.circle({16, 16}, 5, tgx::colors::white);

    const tgx::Image image = target.read();
    int strays = 0;
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const float distance = std::hypot(static_cast<float>(x) + 0.5f - 128.f, static_cast<float>(y) + 0.5f - 128.f);
            const bool white = image[x, y].r > 128;
            if (white != (distance < 40.f) && std::abs(distance - 40.f) > 1.f) {
                ++strays;
            }
        }
    }
    CHECK(strays == 0);
}

TEST_CASE("a circle far larger than what it draws into covers it") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.circle({32, 32}, 1e8f, tgx::colors::white);

    CHECK(target.read()[5, 5] == tgx::colors::white);
}

TEST_CASE("additive blending adds up") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.set_blend(tgx::Blend::additive);
    canvas.rect({0, 0, 20, 20}, tgx::colors::red);
    canvas.rect({0, 0, 20, 20}, tgx::colors::green);

    CHECK(target.read()[10, 10] == tgx::colors::yellow);
}

TEST_CASE("line_strip: a mitered corner") {
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const std::array corner{tgx::Vec2{10, 10}, tgx::Vec2{210, 10}, tgx::Vec2{210, 210}};

    canvas.clear(tgx::colors::black);
    canvas.line_strip(corner, tgx::colors::green, 10.f);

    const tgx::Image image = target.read();
    CHECK(image[110, 10] == tgx::colors::green);
    CHECK(image[210, 110] == tgx::colors::green);
    CHECK(image[213, 7] == tgx::colors::green);
    CHECK(image[218, 2] == tgx::colors::black);
}

TEST_CASE("line_strip: a hairpin turn has no spike") {
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const std::array hairpin{tgx::Vec2{10, 100}, tgx::Vec2{210, 100}, tgx::Vec2{10, 112}};

    canvas.clear(tgx::colors::black);
    canvas.line_strip(hairpin, tgx::colors::white, 8.f);

    const tgx::Image image = target.read();
    CHECK(image[208, 102] == tgx::colors::white);
    CHECK(image[235, 106] == tgx::colors::black);
}

TEST_CASE("line_strip: a repeated point is skipped, a single one draws nothing") {
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const std::array repeated{tgx::Vec2{60, 150}, tgx::Vec2{60, 150}, tgx::Vec2{160, 150}};
    const std::array single{tgx::Vec2{200, 200}};

    canvas.clear(tgx::colors::black);
    canvas.line_strip(repeated, tgx::colors::yellow, 4.f);
    canvas.line_strip(single, tgx::colors::yellow, 4.f);

    const tgx::Image image = target.read();
    CHECK(image[110, 150] == tgx::colors::yellow);
    CHECK(image[200, 200] == tgx::colors::black);
}

TEST_CASE("rect_gradient, top to bottom and by the corners") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.rect_gradient({0, 0, 64, 64}, tgx::colors::red, tgx::colors::blue);
    tgx::Image image = target.read();
    CHECK(tgx_test::near(image[32, 0], tgx::colors::red, 8));
    CHECK(tgx_test::near(image[32, 63], tgx::colors::blue, 8));
    CHECK(tgx_test::near(image[32, 32], tgx::Color{128, 0, 128, 255}, 8));

    canvas.clear(tgx::colors::black);
    canvas.rect_gradient(
        {0, 0, 64, 64},
        tgx::colors::red, tgx::colors::green, tgx::colors::blue, tgx::colors::white
    );
    image = target.read();
    CHECK(tgx_test::near(image[0, 0], tgx::colors::red, 16));
    CHECK(tgx_test::near(image[63, 0], tgx::colors::green, 16));
    CHECK(tgx_test::near(image[63, 63], tgx::colors::blue, 16));
    CHECK(tgx_test::near(image[0, 63], tgx::colors::white, 16));
}

TEST_CASE("triangle_gradient, a color at each corner") {
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.triangle_gradient({10, 10}, {210, 10}, {110, 170}, tgx::colors::red, tgx::colors::green, tgx::colors::blue);

    const tgx::Image image = target.read();
    CHECK(tgx_test::near(image[13, 12], tgx::colors::red, 16));
    CHECK(tgx_test::near(image[206, 12], tgx::colors::green, 16));
    CHECK(tgx_test::near(image[110, 166], tgx::colors::blue, 16));
}

TEST_CASE("circle_gradient, from the center out") {
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.circle_gradient({128, 128}, 80, tgx::colors::white, tgx::colors::black);
    tgx::Image image = target.read();
    CHECK(image[128, 128].r > 245);
    CHECK(image[205, 128].r < 20);
    CHECK(tgx_test::near(image[168, 128], tgx::Color{128, 128, 128, 255}, 18));

    canvas.clear(tgx::colors::black);
    canvas.circle_gradient({128, 128}, 50, tgx::colors::yellow, tgx::colors::yellow.fade(0.f));
    image = target.read();
    CHECK(tgx_test::near(image[128, 128], tgx::colors::yellow, 8));
    CHECK(image[183, 128] == tgx::colors::black);
}
