#include <cstdio>
#include <format>

#include "tgx/tgx.h"

namespace {
    constexpr const char *title = "tgx - 01 window";
}

int main() {
    auto platform = tgx::Platform::create();
    if (!platform) {
        std::fprintf(stderr, "platform: %s\n", tgx::to_str(platform.error()).data());
        return 1;
    }

    auto window = tgx::Window::create(*platform, {.title = title});
    if (!window) {
        std::fprintf(stderr, "window: %s\n", tgx::to_str(window.error()).data());
        return 1;
    }

    tgx::Device device{*window};
    device.set_clear_color(tgx::colors::green);

    tgx::Clock clock;

    while (!window->should_close()) {
        clock.tick();

        if (clock.fps_updated()) {
            window->set_title(std::format("{} - {:.0f} fps", title, clock.fps()));
        }

        const auto [width, height] = window->framebuffer_size();
        device.set_viewport(0, 0, width, height);

        device.clear();

        window->swap_buffers();
        platform->poll_events();
    }

    return 0;
}
