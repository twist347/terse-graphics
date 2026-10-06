// Movement that does not depend on the frame rate: speed in units per second,
// times the seconds the last frame took, clock().delta().

#include "tgx/tgx.h"

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

    while (!app->should_close()) {
        app->poll_events();

        x += speed * app->clock().delta();
        // Off the right edge: back in from the left.
        if (x > static_cast<float>(canvas.size().width)) {
            x = -side;
        }

        canvas.clear(tgx::colors::dark_gray);
        canvas.rect({x, 300, side, side}, tgx::colors::yellow);

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
