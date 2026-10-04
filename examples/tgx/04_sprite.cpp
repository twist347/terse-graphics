// What a Sprite can do, one field at a time: take a part of a texture (an
// atlas of two frames here), mirror it, tint it, turn it.

#include "tgx/tgx.h"

#include <cstdio>
#include <print>

int main() {
    auto app = tgx::App::create({.title = "tgx - 04 sprite"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    // Two 8x8 frames side by side: an arrow and a cross. Each character is a
    // pixel, '#' white, ' ' transparent.
    constexpr const char *art[8]{
        "    #      ##   ",
        "    ##     ##   ",
        "#######    ##   ",
        "######## #######",
        "######## #######",
        "#######    ##   ",
        "    ##     ##   ",
        "    #      ##   ",
    };
    auto image = tgx::Image::create({16, 8});
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 16; ++x) {
            image.at(x, y) = art[y][x] == '#' ? tgx::colors::white : tgx::colors::transparent;
        }
    }

    auto atlas = tgx::Texture::create(image, {.filter = tgx::TextureFilter::nearest});
    if (!atlas) {
        std::println(stderr, "atlas: {}", atlas.error());
        return 1;
    }

    // The frames, in texels from the atlas's top-left corner.
    constexpr tgx::Rect arrow{0, 0, 8, 8};
    constexpr tgx::Rect cross{8, 0, 8, 8};
    // Everything below is drawn this big.
    constexpr tgx::Vec2 size{128, 128};

    auto &canvas = app->canvas();

    while (!app->should_close()) {
        app->poll_events();

        canvas.clear(tgx::colors::dark_gray);

        // The whole atlas at its own size, for reference.
        canvas.sprite(*atlas, {{100, 60}});

        // One frame each, by src.
        canvas.sprite(*atlas, {.position = {100, 160}, .size = size, .src = arrow});
        canvas.sprite(*atlas, {.position = {280, 160}, .size = size, .src = cross});

        // A negative width mirrors: the arrow points left.
        canvas.sprite(*atlas, {.position = {460, 160}, .size = size, .src = {0, 0, -8, 8}});

        // The tint multiplies the texture's colors.
        canvas.sprite(*atlas, {.position = {640, 160}, .size = size, .src = arrow, .tint = tgx::colors::cyan});

        // Turned 90 degrees clockwise about its middle: origin is the point of
        // the sprite that sits at position and that it turns around.
        canvas.sprite(*atlas, {
            .position = tgx::Vec2{820, 160} + size / 2,
            .size = size,
            .src = arrow,
            .origin = size / 2,
            .rotation = tgx::radians(90),
        });

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
