// Nine-slice: one small frame drawn at any size, corners kept sharp. A panel
// that stretches, buttons as wide as their labels, and a bar cut in three
// that fills up.

#include "tgx/tgx.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <print>

namespace {
    // A 6x6 frame on the left, a 6x4 bar on the right. '#' edge, '+' frame
    // border, '.' frame middle, '=' bar fill, ' ' transparent.
    constexpr std::array<const char *, 6> art{
        " ####       ",
        "#++++# #### ",
        "#+..+##====#",
        "#+..+##====#",
        "#++++# #### ",
        " ####       ",
    };

    constexpr tgx::Rect frame{0, 0, 6, 6};
    constexpr tgx::Rect bar{6, 1, 6, 4};
    // Texels drawn 4 times as big.
    constexpr float scale = 4;

    constexpr std::array labels{"Play", "Options", "Quit"};
    constexpr float label_size = 32;
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 15 nine-slice"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto image = tgx::Image::create({12, 6});
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 12; ++x) {
            switch (art[static_cast<std::size_t>(y)][x]) {
                case '#': image[x, y] = tgx::colors::light_gray; break;
                case '+': image[x, y] = tgx::colors::gray; break;
                case '.': image[x, y] = tgx::Color{40, 44, 60, 255}; break;
                case '=': image[x, y] = tgx::colors::white; break;
                default: image[x, y] = tgx::colors::transparent; break;
            }
        }
    }
    auto atlas = tgx::Texture::create(image, {.filter = tgx::TextureFilter::nearest});
    if (!atlas) {
        std::println(stderr, "atlas: {}", atlas.error());
        return 1;
    }

    auto &window = app->window();
    const auto &input = app->input();
    auto &canvas = app->canvas();

    while (!app->should_close()) {
        app->poll_events();
        const auto time = static_cast<float>(app->clock().elapsed());

        canvas.clear(tgx::colors::dark_gray);

        // The atlas itself, for reference.
        canvas.text({40, 40}, "the texture, 12x6:", tgx::colors::light_gray);
        canvas.sprite(*atlas, {.position = {200, 36}, .size = {12 * scale, 6 * scale}});

        // Borders 2 texels each way: 8 on the canvas, whatever the size.
        const float width = std::lerp(200.f, 700.f, (1.f - std::cos(time)) / 2.f);
        const tgx::Rect panel{40, 100, width, 140};
        canvas.nine_slice(*atlas, {
            .position = panel.position(),
            .size = panel.size(),
            .src = frame,
            .borders = {2, 2, 2, 2},
            .border_scale = scale,
        });
        canvas.text(panel.center(), "stretches", tgx::colors::white, label_size, {0.5f, 0.5f});

        // Each button as wide as its label; the one under the mouse lights up.
        auto shape = tgx::CursorShape::arrow;
        float x = 40;
        for (const char *label: labels) {
            const tgx::Vec2 extent = tgx::Canvas::measure_text(label, label_size);
            const tgx::Rect button{x, 300, extent.x + 48, extent.y + 32};
            const bool over = tgx::contains(button, input.mouse());
            if (over) {
                shape = tgx::CursorShape::hand;
            }
            canvas.nine_slice(*atlas, {
                .position = button.position(),
                .size = button.size(),
                .src = frame,
                .borders = {2, 2, 2, 2},
                .border_scale = scale,
                .tint = over ? tgx::colors::yellow : tgx::colors::white,
            });
            canvas.text(button.center(), label, tgx::colors::white, label_size, {0.5f, 0.5f});
            x = button.right() + 20;
        }
        window.set_cursor_shape(shape);

        // Borders only left and right: cut in three, the ends kept round.
        constexpr tgx::NineSlice track{
            .position = {40, 440},
            .size = {700, bar.height * scale},
            .src = bar,
            .borders = {2, 0, 2, 0},
            .border_scale = scale,
            .tint = tgx::colors::gray,
        };
        canvas.nine_slice(*atlas, track);
        tgx::NineSlice fill = track;
        fill.size.x *= tgx::wrap(time / 4.f, 1.f);
        fill.tint = tgx::colors::green;
        canvas.nine_slice(*atlas, fill);
        canvas.text({40, 470}, "shorter than its two ends, they shrink to fit", tgx::colors::light_gray);

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
