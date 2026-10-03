#include "tgx/tgx.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <numbers>
#include <print>
#include <string_view>

namespace {
    constexpr const char *title = "tgx - 10 sprites";

    // Two 8x8 frames side by side: a ghost looking left, then right, its hem
    // swaying. Each character is a pixel: '#' body, 'o' eye, '.' pupil, ' '
    // clear.
    constexpr std::array<std::string_view, 8> art{
        "  ####    ####  ",
        " ######  ###### ",
        "#oo##oo##oo##oo#",
        "#.o##.o##o.##o.#",
        "################",
        "################",
        "################",
        "## ## ### ## ## ",
    };

    constexpr int frame_size = 8;

    // Pixel art drawn big: 8 texels become 64 units. The texture is nearest
    // filtered, so they stay sharp squares.
    constexpr float scale = 8.f;

    [[nodiscard]] auto make_atlas() -> tgx::Image {
        auto image = tgx::Image::create({2 * frame_size, frame_size});
        for (int y = 0; y < frame_size; ++y) {
            for (int x = 0; x < 2 * frame_size; ++x) {
                const char c = art[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                image.at(x, y) = c == '#' ? tgx::colors::white
                    : c == 'o' ? tgx::colors::light_gray
                    : c == '.' ? tgx::colors::black
                    : tgx::colors::transparent;
            }
        }
        return image;
    }

    // Frame i of the atlas, in texels.
    [[nodiscard]] constexpr auto frame(int i) noexcept -> tgx::Rect {
        return {static_cast<float>(i * frame_size), 0.f, frame_size, frame_size};
    }
}

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = title});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto atlas = tgx::Texture::create(make_atlas(), {.filter = tgx::TextureFilter::nearest});
    if (!atlas) {
        std::println(stderr, "texture: {}", atlas.error());
        return 1;
    }

    auto &canvas = app->canvas();
    constexpr tgx::Vec2 big{frame_size * scale, frame_size * scale};
    constexpr tgx::Vec2 middle = big / 2.f;

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        const auto t = static_cast<float>(app->clock().elapsed());
        const int look = static_cast<int>(t * 2.f) % 2;   // switches frame twice a second
        const tgx::Size size = canvas.size();
        const tgx::Vec2 screen_center{static_cast<float>(size.width) / 2.f, static_cast<float>(size.height) / 2.f};

        canvas.clear(tgx::colors::dark_gray);

        // The world, seen through a camera that sways and breathes around the
        // middle of the screen.
        canvas.set_camera({
            .target = {0.f, 0.f},
            .offset = screen_center,
            .rotation = 0.15f * std::sin(t * 0.7f),
            .zoom = 1.f + 0.2f * std::sin(t * 0.5f),
        });

        canvas.rect({-400.f, -200.f, 800.f, 400.f}, tgx::colors::black.with_alpha(80));

        // The whole atlas at its own size, then the same frames big: as is,
        // mirrored, tinted, and turning about its middle.
        canvas.sprite(*atlas, {{-380.f, -180.f}});
        canvas.sprite(*atlas, {.position = {-300.f, -100.f}, .size = big, .src = frame(look)});
        canvas.sprite(*atlas, {
            .position = {-160.f, -100.f},
            .size = big,
            .src = {frame(look).x, 0.f, -frame_size, frame_size},
        });
        canvas.sprite(*atlas, {.position = {-20.f, -100.f}, .size = big, .src = frame(look), .tint = tgx::colors::cyan});
        canvas.sprite(*atlas, {
            .position = tgx::Vec2{120.f, -100.f} + middle,
            .size = big,
            .src = frame(look),
            .origin = middle,
            .rotation = t,
        });

        // A row of them fading in, one draw for all of them: same texture.
        for (int i = 0; i < 10; ++i) {
            const auto alpha = static_cast<std::uint8_t>(25 * (i + 1));
            canvas.sprite(*atlas, {
                .position = {-380.f + 76.f * static_cast<float>(i), 60.f},
                .size = big,
                .src = frame((i + look) % 2),
                .tint = tgx::colors::white.with_alpha(alpha),
            });
        }

        // Back to plain screen coordinates for what stays put.
        canvas.set_camera({});
        canvas.rect({0.f, 0.f, static_cast<float>(size.width), 32.f}, tgx::colors::black.with_alpha(160));
        canvas.sprite(*atlas, {.position = {8.f, 4.f}, .size = {24.f, 24.f}, .src = frame(0)});

        app->swap_buffers();
    }

    return 0;
}
