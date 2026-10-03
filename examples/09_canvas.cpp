#include "tgx/tgx.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <format>
#include <numbers>
#include <print>

namespace {
    constexpr const char *title = "tgx - 09 canvas";

    constexpr std::array palette{
        tgx::colors::red,
        tgx::colors::yellow,
        tgx::colors::green,
        tgx::colors::cyan,
        tgx::colors::blue,
        tgx::colors::magenta,
    };
}

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = title});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    // No shaders, buffers or vertex arrays: the canvas has its own.
    auto &canvas = app->canvas();

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        const auto t = static_cast<float>(app->clock().elapsed());

        canvas.clear(tgx::colors::dark_gray);

        // A row of squares, in screen coordinates from the top-left corner.
        for (std::size_t i = 0; i < palette.size(); ++i) {
            canvas.rect({40.f + 110.f * static_cast<float>(i), 40.f, 90.f, 90.f}, palette[i]);
        }

        // Later shapes cover earlier ones: drawn in call order.
        canvas.rect({40.f, 180.f, 300.f, 200.f}, tgx::colors::light_gray);
        canvas.triangle({190.f, 200.f}, {320.f, 360.f}, {60.f, 360.f}, tgx::colors::red.with_alpha(160));

        // Lines from 1 to 8 units thick.
        for (int i = 0; i < 8; ++i) {
            const float y = 200.f + 24.f * static_cast<float>(i);
            canvas.line({400.f, y}, {640.f, y}, tgx::colors::white, static_cast<float>(i + 1));
        }

        // A dial with a hand going round once every four seconds.
        constexpr tgx::Vec2 center{900.f, 300.f};
        const float angle = t * (std::numbers::pi_v<float> / 2.f);
        canvas.circle(center, 140.f, tgx::colors::black.with_alpha(100));
        canvas.circle_lines(center, 140.f, tgx::colors::white, 4.f);
        canvas.line(center, center + tgx::Vec2{std::cos(angle), std::sin(angle)} * 120.f, tgx::colors::yellow, 6.f);
        canvas.circle(center, 8.f, tgx::colors::white);

        // Filled and outlined, from small to large: circles get as many
        // segments as their size needs.
        for (std::size_t i = 0; i < palette.size(); ++i) {
            const float x = 60.f + 110.f * static_cast<float>(i);
            const float radius = 8.f + 7.f * static_cast<float>(i);
            canvas.circle({x, 520.f}, radius, palette[i]);
            canvas.circle_lines({x, 640.f}, radius, palette[i], 3.f);
        }
        canvas.rect_lines({760.f, 480.f, 280.f, 200.f}, tgx::colors::white, 2.f);
        canvas.rect_lines({780.f, 500.f, 240.f, 160.f}, tgx::colors::cyan.with_alpha(128), 12.f);

        app->swap_buffers();
    }

    return 0;
}
