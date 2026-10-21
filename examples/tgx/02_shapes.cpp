// The canvas shapes, one call each. Coordinates are the window's: (0, 0) at
// the top-left corner, x to the right, y down. Angles are radians, clockwise
// on screen from the right.

#include "tgx/tgx.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <format>
#include <numbers>
#include <print>

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    // 4 samples a pixel: smooth edges on round and turned shapes.
    auto app = tgx::App::create({.title = "tgx - 02 shapes", .msaa = 4});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &canvas = app->canvas();
    constexpr float pi = std::numbers::pi_v<float>;

    while (!app->should_close()) {
        app->poll_events();

        const auto time = static_cast<float>(app->clock().elapsed());

        canvas.clear(tgx::colors::dark_gray);

        // Filled: a rectangle is {x, y, width, height}.
        canvas.rect({28, 60, 200, 130}, tgx::colors::red);
        // A glow first, fading to see-through, then the circle over it.
        canvas.circle_gradient({384, 125}, 90, tgx::colors::yellow.fade(0.4f), tgx::colors::yellow.fade(0));
        canvas.circle({384, 125}, 60, tgx::colors::yellow);
        canvas.triangle({540, 190}, {640, 60}, {740, 190}, tgx::colors::green);
        canvas.rect_rounded({796, 60, 200, 130}, 24, tgx::colors::blue);
        // A regular polygon: center, sides, radius; a turn last, optional.
        canvas.poly({1152, 125}, 6, 70, tgx::colors::purple);

        // Outlines take a thickness and keep inside the shape's edges.
        canvas.rect_lines({28, 230, 200, 130}, tgx::colors::cyan, 3);
        canvas.circle_lines({384, 295}, 65, tgx::colors::magenta, 3);
        canvas.triangle_lines({540, 360}, {640, 230}, {740, 360}, tgx::colors::green, 3);
        canvas.rect_rounded_lines({796, 230, 200, 130}, 24, tgx::colors::light_gray, 3);
        canvas.poly_lines({1152, 295}, 5, 70, tgx::colors::pink, 3, -pi / 2);

        // Turned around a point of its own: here its middle.
        canvas.rect({128, 465, 160, 40}, tgx::colors::orange, time, {80, 20});
        // Slices and arcs go from one angle to another.
        canvas.circle_sector({384, 465}, 65, pi / 4, 7 * pi / 4, tgx::colors::yellow);
        canvas.ring({640, 465}, 45, 65, 0, 2 * pi, tgx::colors::gray);
        canvas.ring({640, 465}, 45, 65, -pi / 2, -pi / 2 + tgx::wrap(time, 2 * pi), tgx::colors::green);

        // Joined lines: corners without gaps. line_loop closes the shape.
        constexpr std::array zigzag{
            tgx::Vec2{796, 530}, tgx::Vec2{846, 400}, tgx::Vec2{896, 530},
            tgx::Vec2{946, 400}, tgx::Vec2{996, 530},
        };
        canvas.line_strip(zigzag, tgx::colors::orange, 6);
        std::array<tgx::Vec2, 10> star{};
        for (std::size_t i = 0; i < star.size(); ++i) {
            const float radius = i % 2 == 0 ? 70.f : 30.f;
            star[i] = tgx::Vec2{1152, 465} + tgx::from_angle(-pi / 2 + pi * static_cast<float>(i) / 5) * radius;
        }
        canvas.line_loop(star, tgx::colors::yellow, 4);

        canvas.line({28, 635}, {228, 635}, tgx::colors::white, 4);

        // Gradients: a color for each corner, blended between them.
        canvas.rect_gradient(
            {284, 570, 200, 130},
            tgx::colors::red, tgx::colors::yellow, tgx::colors::blue, tgx::colors::green
        );
        canvas.triangle_gradient(
            {540, 700}, {640, 570}, {740, 700},
            tgx::colors::red, tgx::colors::green, tgx::colors::blue
        );

        // Later shapes cover earlier ones; alpha lets them show through.
        canvas.rect({816, 570, 110, 110}, tgx::colors::blue);
        canvas.rect({866, 590, 110, 110}, tgx::colors::red.with_alpha(128));

        // Ellipses: radii along their own axes, then a turn. A planet with a
        // band and a tilted ring.
        canvas.circle({1152, 635}, 45, tgx::colors::brown);
        canvas.ellipse({1152, 635}, {45, 12}, tgx::colors::orange, -0.3f);
        canvas.ellipse_lines({1152, 635}, {110, 30}, tgx::colors::light_gray, 6, -0.3f);

        // All the shapes above share one texture, shader and blend: one draw.
        const tgx::FrameStats stats = app->device().stats();
        canvas.text({10, 30}, std::format("{} draws", stats.draws), tgx::colors::white);
        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
