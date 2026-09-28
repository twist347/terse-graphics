#include <cstdio>
#include <format>

#include "tgx/tgx.h"

namespace {
    constexpr const char *title = "tgx - 01 window";
}

int main() {
    auto app = tgx::App::create({.title = title});
    if (!app) {
        std::fprintf(stderr, "app: %s\n", tgx::to_str(app.error()));
        return 1;
    }

    app->device().set_clear_color(tgx::colors::cyan);

    while (app->next_frame()) {
        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        app->device().clear();
    }

    return 0;
}
