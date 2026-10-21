// Clipping: shapes cut to a rect, their coordinates unchanged. A list that
// scrolls inside a panel, a world turning behind a window that stays upright,
// a bar shown up to how full it is. C turns the clips off to show what they
// cut away.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <optional>
#include <print>

namespace {
    constexpr tgx::Rect list{60, 120, 360, 480};
    constexpr float row_height = 40;
    constexpr int row_count = 30;

    constexpr tgx::Rect porthole{500, 120, 360, 480};

    constexpr tgx::Rect bar{940, 120, 280, 48};
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 16 clip"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    const auto &input = app->input();
    auto &canvas = app->canvas();
    // The same layout in any window; the clips scale with it.
    canvas.set_size({1280, 720});
    tgx::Canvas world = canvas;

    float scroll = 0;
    bool clipping = true;
    const auto clip_to = [&](tgx::Canvas &target, tgx::Rect rect) {
        target.set_clip(clipping ? std::optional{rect} : std::nullopt);
    };

    while (!app->should_close()) {
        app->poll_events();
        const auto time = static_cast<float>(app->clock().elapsed());

        if (input.pressed(tgx::Key::c)) {
            clipping = !clipping;
        }
        const tgx::Vec2 mouse = canvas.to_world(input.mouse());
        const bool over_list = tgx::contains(list, mouse);
        if (over_list) {
            const float end = row_count * row_height - list.height;
            scroll = std::clamp(scroll - input.wheel().y * row_height, 0.f, end);
        }

        canvas.clear(tgx::colors::dark_gray);
        canvas.text({60, 40}, "the wheel over the list scrolls it, C turns the clips off", tgx::colors::light_gray);

        // Rows past the panel's edges are cut, half rows too.
        canvas.rect(list, {30, 34, 48, 255});
        clip_to(canvas, list);
        for (int i = 0; i < row_count; ++i) {
            const tgx::Rect row{list.x, list.y + static_cast<float>(i) * row_height - scroll, list.width, row_height};
            tgx::Color color = i % 2 == 0 ? tgx::Color{44, 50, 70, 255} : tgx::Color{38, 43, 60, 255};
            if (over_list && tgx::contains(row, mouse)) {
                color = {70, 90, 150, 255};
            }
            canvas.rect(row, color);
            canvas.text({row.x + 16, row.center().y}, std::format("item {}", i + 1), tgx::colors::white, 24, {0, 0.5f});
        }
        canvas.set_clip({});
        canvas.rect_lines(list, tgx::colors::light_gray, 2);
        canvas.text({list.x, list.bottom() + 16}, "a list in a panel", tgx::colors::light_gray);

        // The camera turns the world, the clip stays an upright rect.
        world.set_camera({.offset = porthole.center(), .rotation = time * 0.3f});
        clip_to(world, porthole);
        for (int y = -6; y <= 6; ++y) {
            for (int x = -6; x <= 6; ++x) {
                const tgx::Vec2 cell{static_cast<float>(x) * 60, static_cast<float>(y) * 60};
                const auto hue = static_cast<float>((x + y + 12) * 15);
                world.rect(tgx::Rect::from_center(cell, {40, 40}), tgx::Color::hsv(hue, 0.6f, 0.9f));
            }
        }
        canvas.rect_lines(porthole, tgx::colors::light_gray, 2);
        canvas.text({porthole.x, porthole.bottom() + 16}, "a world through a window", tgx::colors::light_gray);

        // Drawn whole, shown up to how full it is: the text is cut too.
        const float full = (1.f - std::cos(time)) / 2.f;
        canvas.rect(bar, tgx::colors::black);
        clip_to(canvas, {bar.x, bar.y, bar.width * full, bar.height});
        canvas.rect_gradient(bar, tgx::colors::red, tgx::colors::green, tgx::colors::green, tgx::colors::red);
        canvas.text(bar.center(), "LOADING", tgx::colors::black, 32, {0.5f, 0.5f});
        canvas.set_clip({});
        canvas.rect_lines(bar, tgx::colors::light_gray, 2);
        canvas.text({bar.x, bar.bottom() + 16}, "a bar, empty at 0", tgx::colors::light_gray);

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
