// A minimap: the same world drawn a second time, into a corner of the window.
// set_viewport picks the corner, set_size how much of the world fits in it.

#include "tgx/tgx.h"

#include <array>
#include <cstdio>
#include <print>

namespace {
    constexpr tgx::Vec2 world_size{2400, 600};

    constexpr std::array trees{
        tgx::Vec2{150, 150}, tgx::Vec2{450, 400}, tgx::Vec2{750, 200}, tgx::Vec2{1050, 450},
        tgx::Vec2{1350, 150}, tgx::Vec2{1650, 400}, tgx::Vec2{1950, 200}, tgx::Vec2{2250, 450},
    };

    // The same calls for both views: each canvas puts them where it shows.
    auto draw_world(tgx::Canvas &canvas, tgx::Vec2 player) noexcept -> void {
        canvas.rect({0, 0, world_size.x, world_size.y}, tgx::colors::black);
        for (const tgx::Vec2 tree : trees) {
            canvas.circle(tree, 40, tgx::colors::green);
        }
        canvas.rect({player.x - 10, player.y - 10, 20, 20}, tgx::colors::yellow);
    }
}

int main() {
    // Info also prints which GL context the driver gave.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 07 viewport"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &screen = app->canvas();
    tgx::Canvas main_view = screen;

    // A 400x100 corner of the window that spans the whole world: 2400x600
    // units squeezed into it.
    constexpr tgx::Rect minimap_area{20, 40, 400, 100};
    tgx::Canvas minimap = screen;
    minimap.set_viewport(minimap_area);
    minimap.set_size({static_cast<int>(world_size.x), static_cast<int>(world_size.y)});

    tgx::Vec2 player{0, 300};

    while (!app->should_close()) {
        app->poll_events();

        player.x += 200 * app->clock().delta();
        if (player.x > world_size.x) {
            player.x = 0;
        }

        const tgx::Size size = screen.size();
        main_view.set_camera({
            .target = player,
            .offset = {static_cast<float>(size.width) / 2, static_cast<float>(size.height) / 2},
            .zoom = 1.5f,
        });

        screen.clear(tgx::colors::dark_gray);
        draw_world(main_view, player);
        draw_world(minimap, player);
        // A frame around the minimap, in window coordinates.
        screen.rect_lines(minimap_area, tgx::colors::white, 2);

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
