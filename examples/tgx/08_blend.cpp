// How overlapping colors combine. Left: alpha blending, the default, where
// what is on top covers what is below. Right: additive, where light adds up,
// red + green + blue making white.

#include "tgx/tgx.hpp"

#include <cstdio>
#include <print>

namespace {
    auto draw_lights(tgx::Canvas &canvas, tgx::Vec2 center) noexcept -> void {
        canvas.circle(center + tgx::Vec2{0, -60}, 120, tgx::colors::red.with_alpha(200));
        canvas.circle(center + tgx::Vec2{-60, 50}, 120, tgx::colors::green.with_alpha(200));
        canvas.circle(center + tgx::Vec2{60, 50}, 120, tgx::colors::blue.with_alpha(200));
    }
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 08 blend"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &canvas = app->canvas();
    tgx::Canvas additive = canvas;
    additive.set_blend(tgx::Blend::additive);

    while (!app->should_close()) {
        app->poll_events();

        canvas.clear(tgx::colors::black);
        draw_lights(canvas, {340, 360});
        draw_lights(additive, {940, 360});

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
