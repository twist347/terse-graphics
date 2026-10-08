// The canvas shapes, one call each. Coordinates are the window's: (0, 0) at
// the top-left corner, x to the right, y down.

#include "tgx/tgx.hpp"

#include <array>
#include <cstdio>
#include <print>

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 02 shapes"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &canvas = app->canvas();

    while (!app->should_close()) {
        app->poll_events();

        canvas.clear(tgx::colors::dark_gray);

        // Filled: a rectangle is {x, y, width, height}.
        canvas.rect({100, 100, 240, 160}, tgx::colors::red);
        // A glow first, fading to see-through, then the circle over it.
        canvas.circle_gradient({540, 180}, 120, tgx::colors::yellow.fade(0.4f), tgx::colors::yellow.fade(0));
        canvas.circle({540, 180}, 80, tgx::colors::yellow);
        canvas.triangle({760, 260}, {860, 100}, {960, 260}, tgx::colors::green);

        // Lines and outlines take a thickness.
        canvas.line({100, 340}, {960, 340}, tgx::colors::white, 4);
        canvas.rect_lines({100, 420, 240, 160}, tgx::colors::cyan, 3);
        canvas.circle_lines({540, 500}, 80, tgx::colors::magenta, 3);

        // Gradients: a color for each corner, blended between them.
        canvas.rect_gradient(
            {1020, 100, 200, 160},
            tgx::colors::red, tgx::colors::yellow, tgx::colors::blue, tgx::colors::green
        );
        canvas.triangle_gradient(
            {1020, 580}, {1120, 420}, {1220, 580},
            tgx::colors::red, tgx::colors::green, tgx::colors::blue
        );

        // Joined lines: corners without gaps.
        const std::array zigzag{
            tgx::Vec2{1020, 380}, tgx::Vec2{1070, 300}, tgx::Vec2{1120, 380},
            tgx::Vec2{1170, 300}, tgx::Vec2{1220, 380},
        };
        canvas.line_strip(zigzag, tgx::colors::orange, 6);

        // Later shapes cover earlier ones; alpha lets them show through.
        canvas.rect({760, 420, 140, 140}, tgx::colors::blue);
        canvas.rect({820, 460, 140, 140}, tgx::colors::red.with_alpha(128));

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
