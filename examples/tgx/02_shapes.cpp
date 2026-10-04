// The canvas shapes, one call each. Coordinates are the window's: (0, 0) at
// the top-left corner, x to the right, y down.

#include "tgx/tgx.h"

#include <cstdio>
#include <print>

int main() {
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
        canvas.circle({540, 180}, 80, tgx::colors::yellow);
        canvas.triangle({760, 260}, {860, 100}, {960, 260}, tgx::colors::green);

        // Lines and outlines take a thickness.
        canvas.line({100, 340}, {960, 340}, tgx::colors::white, 4);
        canvas.rect_lines({100, 420, 240, 160}, tgx::colors::cyan, 3);
        canvas.circle_lines({540, 500}, 80, tgx::colors::magenta, 3);

        // Later shapes cover earlier ones; alpha lets them show through.
        canvas.rect({760, 420, 140, 140}, tgx::colors::blue);
        canvas.rect({820, 460, 140, 140}, tgx::colors::red.with_alpha(128));

        app->swap_buffers();
    }

    return 0;
}
