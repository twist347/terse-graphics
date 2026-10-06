// The smallest tgx program: a window cleared to one color, frame after frame,
// until it is closed.

#include "tgx/tgx.h"

#include <cstdio>
#include <print>

int main() {
    // Info also prints which GL context the driver gave.
    tgx::set_log_level(tgx::LogLevel::info);

    // The window, the GL device behind it, a canvas to draw on and the audio,
    // in one go.
    auto app = tgx::App::create({.title = "tgx - 01 window"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    while (!app->should_close()) {
        app->poll_events();

        app->canvas().clear(tgx::colors::dark_gray);

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
