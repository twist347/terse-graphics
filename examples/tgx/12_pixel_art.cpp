// Pixel art: the world is drawn into a small render target, 320x180, which
// is then scaled up onto the window by a whole factor with nearest filtering.
// Every pixel stays a sharp square, even along a turning line or a circle.

#include "tgx/tgx.h"

#include <cmath>
#include <cstdio>
#include <print>

int main() {
    // Info also prints which GL context the driver gave.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 12 pixel art"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    constexpr tgx::Size resolution{320, 180};
    constexpr float width = static_cast<float>(resolution.width);
    constexpr float height = static_cast<float>(resolution.height);
    auto pixels = tgx::RenderTarget::create(resolution, {.filter = tgx::TextureFilter::nearest});
    if (!pixels) {
        std::println(stderr, "render target: {}", pixels.error());
        return 1;
    }

    auto &screen = app->canvas();
    // Draws into the target: its coordinates are the target's 320x180 pixels.
    tgx::Canvas world = screen;
    world.set_target(&*pixels);

    while (!app->should_close()) {
        app->poll_events();

        // The largest whole scale that fits the window, in the middle of it.
        const tgx::Rect picture = tgx::fit_whole(resolution, screen.size());
        const tgx::Vec2 position{picture.x, picture.y};

        world.clear(tgx::Color::rgb(0x305080));
        world.rect({0, 140, width, height - 140}, tgx::colors::brown);
        world.circle({260, 40}, 20, tgx::colors::yellow);
        const tgx::Vec2 turn = tgx::from_angle(static_cast<float>(app->clock().elapsed())) * 30;
        world.line(tgx::Vec2{160, 100} - turn, tgx::Vec2{160, 100} + turn, tgx::colors::white, 3);

        // The mouse in the target's pixels: the window point, less where the
        // picture starts, scaled down as the picture is scaled up.
        const float to_pixels = picture.width > 0 ? width / picture.width : 0;
        const tgx::Vec2 mouse = (app->input().mouse() - position) * to_pixels;
        world.rect({std::floor(mouse.x), std::floor(mouse.y), 1, 1}, tgx::colors::red);

        // The target as one sprite; the world's shapes are drawn into it
        // first, as they came first.
        screen.clear(tgx::colors::black);
        screen.sprite(pixels->texture(), {.position = position, .size = {picture.width, picture.height}});

        screen.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
