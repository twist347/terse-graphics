// The Canvas's shapes: edges, joints, gradients and blending, checked where
// they show.

#include "gpu.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>

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
    constexpr std::array corner{tgx::Vec2{10, 10}, tgx::Vec2{210, 10}, tgx::Vec2{210, 210}};

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
    constexpr std::array hairpin{tgx::Vec2{10, 100}, tgx::Vec2{210, 100}, tgx::Vec2{10, 112}};

    canvas.clear(tgx::colors::black);
    canvas.line_strip(hairpin, tgx::colors::white, 8.f);

    const tgx::Image image = target.read();
    CHECK(image[208, 102] == tgx::colors::white);
    CHECK(image[235, 106] == tgx::colors::black);
}

TEST_CASE("line_strip: a repeated point is skipped, a single one draws nothing") {
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    constexpr std::array repeated{tgx::Vec2{60, 150}, tgx::Vec2{60, 150}, tgx::Vec2{160, 150}};
    constexpr std::array single{tgx::Vec2{200, 200}};

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

TEST_CASE("rect turned around its origin") {
    const tgx::RenderTarget target = tgx_test::target({128, 128});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    // 80x20 centered at (64, 64), turned a quarter: 20 wide, 80 tall.
    canvas.clear(tgx::colors::black);
    canvas.rect({64, 64, 80, 20}, {40, 10}, std::numbers::pi_v<float> / 2.f, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image[64, 30] == tgx::colors::white);
    CHECK(image[64, 98] == tgx::colors::white);
    CHECK(image[30, 64] == tgx::colors::black);
    CHECK(image[80, 64] == tgx::colors::black);
}

TEST_CASE("rect_rounded: the corners cut, the edges whole") {
    const tgx::RenderTarget target = tgx_test::target({128, 128});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.rect_rounded({10, 10, 100, 60}, 20, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image[12, 12] == tgx::colors::black);
    CHECK(image[107, 67] == tgx::colors::black);
    CHECK(image[11, 40] == tgx::colors::white);
    CHECK(image[60, 11] == tgx::colors::white);
    CHECK(image[60, 40] == tgx::colors::white);
    // On the corner's circle, well inside it.
    CHECK(image[20, 20] == tgx::colors::white);
}

TEST_CASE("rect_rounded_lines: a frame inside the edges, the middle left") {
    const tgx::RenderTarget target = tgx_test::target({128, 128});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.rect_rounded_lines({10, 10, 100, 60}, 20, tgx::colors::white, 4);

    const tgx::Image image = target.read();
    CHECK(image[11, 40] == tgx::colors::white);
    CHECK(image[60, 12] == tgx::colors::white);
    CHECK(image[60, 15] == tgx::colors::black);
    CHECK(image[60, 40] == tgx::colors::black);
    CHECK(image[12, 12] == tgx::colors::black);
    CHECK(image[9, 40] == tgx::colors::black);
}

TEST_CASE("triangle_lines: inside the edges, the corners sharp") {
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    canvas.triangle_lines({10, 10}, {210, 10}, {10, 210}, tgx::colors::white, 6);

    const tgx::Image image = target.read();
    CHECK(image[100, 13] == tgx::colors::white);
    CHECK(image[13, 100] == tgx::colors::white);
    CHECK(image[100, 7] == tgx::colors::black);
    CHECK(image[60, 60] == tgx::colors::black);
    // The sharp corner stays within the triangle: no spike out past it.
    CHECK(image[213, 8] == tgx::colors::black);
    CHECK(image[205, 12] == tgx::colors::white);
}

TEST_CASE("line_loop: the closing corner is joined too") {
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    constexpr std::array square{tgx::Vec2{50, 50}, tgx::Vec2{200, 50}, tgx::Vec2{200, 200}, tgx::Vec2{50, 200}};

    canvas.clear(tgx::colors::black);
    canvas.line_loop(square, tgx::colors::white, 10);

    const tgx::Image image = target.read();
    // Every corner mitered, the first one included: its outer square filled.
    CHECK(image[47, 47] == tgx::colors::white);
    CHECK(image[202, 47] == tgx::colors::white);
    CHECK(image[202, 202] == tgx::colors::white);
    CHECK(image[47, 202] == tgx::colors::white);
    CHECK(image[125, 125] == tgx::colors::black);
}

TEST_CASE("line_loop: a repeated point, the first again at the end, one point") {
    const tgx::RenderTarget target = tgx_test::target({256, 256});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    constexpr std::array repeated{
        tgx::Vec2{50, 50}, tgx::Vec2{200, 50}, tgx::Vec2{200, 50}, tgx::Vec2{200, 200}, tgx::Vec2{50, 50},
    };
    constexpr std::array single{tgx::Vec2{20, 230}};

    canvas.clear(tgx::colors::black);
    canvas.set_blend(tgx::Blend::additive);
    canvas.line_loop(repeated, tgx::Color{100, 100, 100, 255}, 6);
    canvas.line_loop(single, tgx::colors::white, 6);

    const tgx::Image image = target.read();
    CHECK(image[125, 50] == tgx::Color{100, 100, 100, 255});
    CHECK(image[200, 125] == tgx::Color{100, 100, 100, 255});
    CHECK(image[125, 125] == tgx::Color{100, 100, 100, 255});
    // Corners joined, not drawn twice.
    CHECK(image[201, 51] == tgx::Color{100, 100, 100, 255});
    CHECK(image[20, 230] == tgx::colors::black);
}

TEST_CASE("circle_sector: from start to end, clockwise on screen") {
    const tgx::RenderTarget target = tgx_test::target({128, 128});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    constexpr float pi = std::numbers::pi_v<float>;

    // The quarter from the right down to the bottom, given end first.
    canvas.clear(tgx::colors::black);
    canvas.circle_sector({64, 64}, 50, pi / 2.f, 0.f, tgx::colors::white);

    tgx::Image image = target.read();
    CHECK(image[90, 90] == tgx::colors::white);
    CHECK(image[38, 90] == tgx::colors::black);
    CHECK(image[90, 38] == tgx::colors::black);
    CHECK(image[38, 38] == tgx::colors::black);

    // More than a full turn is the circle.
    canvas.clear(tgx::colors::black);
    canvas.circle_sector({64, 64}, 50, 0.f, 10.f, tgx::colors::white);
    image = target.read();
    CHECK(image[38, 38] == tgx::colors::white);
    CHECK(image[90, 38] == tgx::colors::white);
}

TEST_CASE("ring: the band between the radii, over the arc") {
    const tgx::RenderTarget target = tgx_test::target({128, 128});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    constexpr float pi = std::numbers::pi_v<float>;

    // The top half: from the left round to the right.
    canvas.clear(tgx::colors::black);
    canvas.ring({64, 64}, 30, 50, pi, 2.f * pi, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image[64, 24] == tgx::colors::white);
    CHECK(image[64, 64 - 25] == tgx::colors::black);
    CHECK(image[64, 64 - 55] == tgx::colors::black);
    CHECK(image[64, 104] == tgx::colors::black);
}

TEST_CASE("ellipse: the radii along its axes, turned, and its outline") {
    const tgx::RenderTarget target = tgx_test::target({128, 128});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    constexpr float pi = std::numbers::pi_v<float>;

    canvas.clear(tgx::colors::black);
    canvas.ellipse({64, 64}, {50, 20}, tgx::colors::white);
    tgx::Image image = target.read();
    CHECK(image[64 + 48, 64] == tgx::colors::white);
    CHECK(image[64 + 52, 64] == tgx::colors::black);
    CHECK(image[64, 64 + 18] == tgx::colors::white);
    CHECK(image[64, 64 + 22] == tgx::colors::black);

    // A quarter turn stands it up.
    canvas.clear(tgx::colors::black);
    canvas.ellipse({64, 64}, {50, 20}, tgx::colors::white, pi / 2.f);
    image = target.read();
    CHECK(image[64, 64 + 48] == tgx::colors::white);
    CHECK(image[64 + 22, 64] == tgx::colors::black);

    // 5 wide inside the edge, on both axes.
    canvas.clear(tgx::colors::black);
    canvas.ellipse_lines({64, 64}, {50, 20}, tgx::colors::white, 5);
    image = target.read();
    CHECK(image[64 + 47, 64] == tgx::colors::white);
    CHECK(image[64 + 43, 64] == tgx::colors::black);
    CHECK(image[64, 64 - 18] == tgx::colors::white);
    CHECK(image[64, 64 - 14] == tgx::colors::black);
    CHECK(image[64, 64] == tgx::colors::black);
}

TEST_CASE("ellipse_lines thicker than its ends bend: no pixel drawn twice") {
    const tgx::RenderTarget target = tgx_test::target({128, 128});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    // The ends bend with a radius of 15 * 15 / 60 = 3.75, under the thickness.
    canvas.clear(tgx::colors::black);
    canvas.ellipse_lines({64, 64}, {60, 15}, tgx::colors::white.fade(0.5f), 10);

    const tgx::Image image = target.read();
    const std::uint8_t once = image[64 + 55, 64].r;
    CHECK(once > 100);
    CHECK(once < 150);
    int twice = 0;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            if (image[x, y].r > once + 1) {
                ++twice;
            }
        }
    }
    CHECK(twice == 0);
    CHECK(image[64, 64] == tgx::colors::black);
}

TEST_CASE("poly: a square with a corner up, and its frame") {
    const tgx::RenderTarget target = tgx_test::target({128, 128});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    constexpr float pi = std::numbers::pi_v<float>;

    // Corners at right, bottom, left, top: a diamond.
    canvas.clear(tgx::colors::black);
    canvas.poly({64, 64}, 4, 50, 0.f, tgx::colors::white);
    tgx::Image image = target.read();
    CHECK(image[64, 16] == tgx::colors::white);
    CHECK(image[110, 64] == tgx::colors::white);
    CHECK(image[25, 25] == tgx::colors::black);

    // Turned an eighth, an upright square of side 50 * sqrt(2), its frame
    // 5 wide inside the edges.
    canvas.clear(tgx::colors::black);
    canvas.poly_lines({64, 64}, 4, 50, pi / 4.f, tgx::colors::white, 5);
    image = target.read();
    constexpr int edge = 64 - 35;
    CHECK(image[64, edge + 2] == tgx::colors::white);
    CHECK(image[64, edge - 2] == tgx::colors::black);
    CHECK(image[64, edge + 8] == tgx::colors::black);
    CHECK(image[edge + 2, edge + 2] == tgx::colors::white);
}

TEST_CASE("text anchored: the same pixels as placed by hand, line by line") {
    const tgx::RenderTarget target = tgx_test::target({256, 128});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const auto drawn = [&](auto draw) {
        canvas.clear(tgx::colors::black);
        draw();
        return target.read();
    };
    const auto width = [](std::string_view line) { return tgx::Canvas::measure_text(line, 16).x; };

    // Centered: each line by its own width, the block by its height (two
    // lines, 32), the shifts rounded to whole units.
    constexpr tgx::Vec2 center{128, 64};
    const tgx::Image anchored = drawn([&] {
        canvas.text(center, "Hello\nhi", tgx::colors::white, 16, {0.5f, 0.5f});
    });
    const tgx::Image by_hand = drawn([&] {
        canvas.text({center.x - std::round(width("Hello") / 2.f), center.y - 16}, "Hello", tgx::colors::white);
        canvas.text({center.x - std::round(width("hi") / 2.f), center.y}, "hi", tgx::colors::white);
    });
    CHECK(anchored.pixels().size() == by_hand.pixels().size());
    CHECK(std::ranges::equal(anchored.pixels(), by_hand.pixels()));

    // Right-aligned at the top-right corner.
    const tgx::Image right = drawn([&] {
        canvas.text({250, 4}, "1\n200", tgx::colors::white, 16, {1, 0});
    });
    const tgx::Image right_by_hand = drawn([&] {
        canvas.text({250 - width("1"), 4}, "1", tgx::colors::white);
        canvas.text({250 - width("200"), 20}, "200", tgx::colors::white);
    });
    CHECK(std::ranges::equal(right.pixels(), right_by_hand.pixels()));

    // The default anchor draws as before.
    const tgx::Image plain = drawn([&] { canvas.text({10, 10}, "a\nbc", tgx::colors::white); });
    const tgx::Image zero = drawn([&] { canvas.text({10, 10}, "a\nbc", tgx::colors::white, 16, {}); });
    CHECK(std::ranges::equal(plain.pixels(), zero.pixels()));
}
