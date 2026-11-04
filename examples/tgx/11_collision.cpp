// Collision checks. A circle follows the mouse and turns red on what it
// overlaps; a click marks the rect under the mouse; the part of the circle's
// square inside the big rect shows filled. Lines find the nearest points of
// rects, a ray stops at its first hit, and a moving rect stops at a wall.

#include "tgx/tgx.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <print>

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 11 collision"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    const auto &input = app->input();
    auto &canvas = app->canvas();

    constexpr std::array rects{
        tgx::Rect{100, 100, 300, 200},
        tgx::Rect{500, 150, 120, 300},
        tgx::Rect{750, 400, 250, 120},
    };
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
        for (const tgx::Circle &circle: circles) {
            const tgx::Color color = tgx::overlaps(circle, cursor) ? tgx::colors::red : tgx::colors::gray;
            canvas.circle(circle.center, circle.radius, color);
        }

        // intersection: the part two rects share, here the big rect and the
        // square around the cursor.
        const auto square = tgx::Rect::from_center(cursor.center, {cursor.radius * 2, cursor.radius * 2});
        canvas.rect(tgx::intersection(rects[0], square), tgx::colors::cyan.with_alpha(160));

        // closest_point: the point of a shape nearest to another, here of each
        // rect to the mouse. Inside a rect it is the mouse itself.
        for (const tgx::Rect &rect: rects) {
            const tgx::Vec2 nearest = tgx::closest_point(rect, input.mouse());
            canvas.line(input.mouse(), nearest, tgx::colors::light_gray);
            canvas.circle(nearest, 4, tgx::colors::white);
        }

        // raycast: the nearest contact along the ray, not every rect it meets.
        constexpr tgx::Vec2 ray_from{40, 640};
        const tgx::Vec2 ray_direction = input.mouse() - ray_from;
        std::optional<float> nearest_hit{};
        if (ray_direction != tgx::Vec2{}) {
            for (const tgx::Rect &rect: rects) {
                const auto hit = tgx::raycast(rect, ray_from, ray_direction);
                if (hit && (!nearest_hit || *hit < *nearest_hit)) {
                    nearest_hit = hit;
                }
            }
        }
        canvas.line(ray_from, input.mouse(), tgx::colors::orange, 2);
        if (nearest_hit) {
            const tgx::Vec2 at = ray_from + tgx::normalize(ray_direction) * *nearest_hit;
            canvas.circle(at, 7, tgx::colors::yellow);
        }
        canvas.text(ray_from - tgx::Vec2{0, 24}, "raycast", tgx::colors::light_gray);

        // sweep: how much of the movement is possible before the wall. The
        // rect waits at contact until the demonstration starts over.
        constexpr tgx::Rect sweep_start{180, 640, 36, 36};
        constexpr tgx::Rect wall{620, 620, 32, 72};
        constexpr tgx::Vec2 movement{440, 0};
        const auto contact = tgx::sweep(sweep_start, movement, wall);
        const float progress = tgx::wrap(static_cast<float>(app->clock().elapsed()) / 4.f, 1.f);
        const float travelled = contact && *contact < progress ? *contact : progress;
        const tgx::Vec2 swept_at = sweep_start.position() + movement * travelled;
        canvas.rect({swept_at.x, swept_at.y, sweep_start.width, sweep_start.height}, tgx::colors::cyan);
        canvas.rect(wall, tgx::colors::purple);
        canvas.text({180, 610}, "sweep", tgx::colors::light_gray);

        canvas.circle_lines(cursor.center, cursor.radius, tgx::colors::white, 2);
        canvas.text({20, 40}, "Move the mouse over the shapes; click a rect to mark it.", tgx::colors::white);

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
