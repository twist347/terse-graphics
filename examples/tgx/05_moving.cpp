// Movement that does not depend on the frame rate: speed in units per second,
// times the seconds the last frame took, clock().delta(). Timers count the
// same seconds, and easing bends a timer's progress into softer motion.

#include "tgx/tgx.hpp"

#include <array>
#include <cmath>
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

    // The same trip back and forth, once linear and once eased.
    auto trip = tgx::Timer::every(1.5f);
    bool returning = false;

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
        if (trip.tick(dt)) {
            returning = !returning;
        }
        const float t = returning ? 1.f - trip.progress() : trip.progress();

        canvas.clear(tgx::colors::dark_gray);
        canvas.rect({x, 300, side, side}, palette[color]);
        canvas.rect({x, 300 + side + 8, side * next_color.progress(), 6}, tgx::colors::white);

        canvas.text({100, 470}, "linear", tgx::colors::gray);
        canvas.rect({std::lerp(100.f, 1100.f, tgx::ease::linear(t)), 490, 40, 40}, tgx::colors::gray);
        canvas.text({100, 550}, "ease::in_out_cubic", tgx::colors::cyan);
        canvas.rect({std::lerp(100.f, 1100.f, tgx::ease::in_out_cubic(t)), 570, 40, 40}, tgx::colors::cyan);

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
