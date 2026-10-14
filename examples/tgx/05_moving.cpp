// Movement that does not depend on the frame rate: speed in units per second,
// times the seconds the last frame took, clock().delta(). Timers count the
// same seconds.

#include "tgx/tgx.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <print>

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 05 moving"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &canvas = app->canvas();

    constexpr float speed = 300;
    constexpr float side = 80;
    float x = 0;

    // Every half second the square takes the next color; the bar under it
    // fills up until then.
    constexpr std::array palette{tgx::colors::yellow, tgx::colors::orange, tgx::colors::red, tgx::colors::magenta};
    auto next_color = tgx::Timer::every(0.5f);
    std::size_t color = 0;

    while (!app->should_close()) {
        app->poll_events();

        const float dt = app->clock().delta();
        x += speed * dt;
        // Off the right edge: back in from the left.
        if (x > static_cast<float>(canvas.size().width)) {
            x = -side;
        }
        if (next_color.tick(dt)) {
            color = (color + 1) % palette.size();
        }

        canvas.clear(tgx::colors::dark_gray);
        canvas.rect({x, 300, side, side}, palette[color]);
        canvas.rect({x, 300 + side + 8, side * next_color.progress(), 6}, tgx::colors::white);

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
