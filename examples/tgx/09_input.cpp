// Keyboard and mouse. WASD or the arrows move the square, the wheel zooms,
// a click leaves a dot where the mouse is in the world, Escape closes the
// window.

#include "tgx/tgx.h"

#include <cstdio>
#include <print>
#include <vector>

int main() {
    // Info also prints which GL context the driver gave.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 09 input"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    // As of the last poll_events(): read it after polling.
    const auto &input = app->input();

    auto &screen = app->canvas();
    tgx::Canvas world = screen;

    constexpr float speed = 300;
    tgx::Vec2 player{0, 0};
    float zoom = 1;
    std::vector<tgx::Vec2> dots;

    while (!app->should_close()) {
        app->poll_events();

        // direction: from the keys held now (WASD or arrows), length 1, so a
        // diagonal is not faster.
        player += input.direction() * speed * app->clock().delta();

        // pressed: once, on the frame the key goes down.
        if (input.pressed(tgx::Key::escape)) {
            app->window().request_close();
        }

        // A notch of the wheel zooms by a tenth.
        zoom *= 1 + 0.1f * input.wheel().y;

        const tgx::Size size = screen.size();
        world.set_camera({
            .target = player,
            .offset = {static_cast<float>(size.width) / 2, static_cast<float>(size.height) / 2},
            .zoom = zoom,
        });

        // The mouse is in window coordinates; the world canvas, which knows its
        // camera, says what world point is under it.
        const tgx::Vec2 mouse = world.to_world(input.mouse());
        if (input.pressed(tgx::MouseButton::left)) {
            dots.push_back(mouse);
        }

        screen.clear(tgx::colors::dark_gray);

        world.rect_lines({-400, -300, 800, 600}, tgx::colors::gray, 2);
        for (const tgx::Vec2 dot: dots) {
            world.circle(dot, 8, tgx::colors::cyan);
        }
        world.rect({player.x - 20, player.y - 20, 40, 40}, tgx::colors::yellow);
        world.circle_lines(mouse, 16, tgx::colors::white, 2);

        screen.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
