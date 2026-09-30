#include <cstdio>
#include <format>
#include <print>

#include "tgx/tgx.h"

namespace {
    constexpr const char *title = "tgx - 01 window";
}

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = title});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    app->device().set_clear_color(tgx::colors::dark_gray);

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        app->device().clear();

        app->swap_buffers();
    }

    return 0;
}
