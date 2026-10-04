// Collision checks. A circle follows the mouse and turns red on what it
// overlaps; a click marks the rect under the mouse; the part of the circle's
// square that is inside the big rect shows filled.

#include "tgx/tgx.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <print>

int main() {
    auto app = tgx::App::create({.title = "tgx - 11 collision"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    const auto &input = app->input();
    auto &canvas = app->canvas();

    std::array<tgx::Rect, 3> rects{{
        {100, 100, 300, 200},
        {500, 150, 120, 300},
        {750, 400, 250, 120},
    }};
    std::array<bool, 3> marked{};
    constexpr std::array circles{
        tgx::Circle{{900, 180}, 80},
        tgx::Circle{{250, 500}, 60},
    };

    while (!app->should_close()) {
        app->poll_events();

        const tgx::Circle cursor{input.mouse(), 40};

        // contains: a point in a shape, here the mouse in a rect.
        if (input.pressed(tgx::MouseButton::left)) {
            for (std::size_t i = 0; i < rects.size(); ++i) {
                if (tgx::contains(rects[i], input.mouse())) {
                    marked[i] = !marked[i];
                }
            }
        }

        canvas.clear(tgx::colors::dark_gray);

        // overlaps: two shapes, here each shape and the cursor.
        for (std::size_t i = 0; i < rects.size(); ++i) {
            const tgx::Color color = tgx::overlaps(rects[i], cursor) ? tgx::colors::red : tgx::colors::gray;
            canvas.rect(rects[i], color);
            if (marked[i]) {
                canvas.rect_lines(rects[i], tgx::colors::yellow, 4);
            }
        }
        for (const tgx::Circle &circle : circles) {
            const tgx::Color color = tgx::overlaps(circle, cursor) ? tgx::colors::red : tgx::colors::gray;
            canvas.circle(circle.center, circle.radius, color);
        }

        // intersection: the part two rects share, here the big rect and the
        // square around the cursor.
        const tgx::Rect square{cursor.center.x - cursor.radius, cursor.center.y - cursor.radius, cursor.radius * 2, cursor.radius * 2};
        canvas.rect(tgx::intersection(rects[0], square), tgx::colors::cyan.with_alpha(160));

        canvas.circle_lines(cursor.center, cursor.radius, tgx::colors::white, 2);
        canvas.text({20, 40}, "Move the mouse over the shapes; click a rect to mark it.", tgx::colors::white);

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
