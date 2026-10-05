// A camera following a player through a world bigger than the window, and a
// HUD that stays put. Each is its own copy of the canvas: one with the
// camera, one without.

#include "tgx/tgx.h"

#include <array>
#include <cstdio>
#include <print>

int main() {
    // Info also prints which GL context the driver gave.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 06 camera"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &screen = app->canvas();
    tgx::Canvas world = screen;

    // In world coordinates: the camera decides where they show up.
    constexpr std::array trees{
        tgx::Vec2{150, 150}, tgx::Vec2{450, 400}, tgx::Vec2{750, 200}, tgx::Vec2{1050, 450},
        tgx::Vec2{1350, 150}, tgx::Vec2{1650, 400}, tgx::Vec2{1950, 200}, tgx::Vec2{2250, 450},
    };
    constexpr float world_width = 2400;
    tgx::Vec2 player{0, 300};

    while (!app->should_close()) {
        app->poll_events();

        player.x += 200 * app->clock().delta();
        if (player.x > world_width) {
            player.x = 0;
        }

        // The player's position (target) shows up in the middle of the window
        // (offset), one and a half times as big (zoom).
        const tgx::Size size = screen.size();
        world.set_camera({
            .target = player,
            .offset = {static_cast<float>(size.width) / 2, static_cast<float>(size.height) / 2},
            .zoom = 1.5f,
        });

        screen.clear(tgx::colors::dark_gray);

        world.rect({0, 0, world_width, 600}, tgx::colors::black);
        for (const tgx::Vec2 tree : trees) {
            world.circle(tree, 40, tgx::colors::green);
        }
        world.rect({player.x - 10, player.y - 10, 20, 20}, tgx::colors::yellow);

        // The HUD, drawn after the world so it is on top: how far the player
        // has gone.
        screen.rect({20, 40, 300, 20}, tgx::colors::gray);
        screen.rect({20, 40, 300 * player.x / world_width, 20}, tgx::colors::yellow);

        screen.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
