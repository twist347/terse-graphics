// A picture on the screen: an Image made pixel by pixel, uploaded to the GPU
// as a Texture, drawn as a sprite. Image::load reads one from a file instead.

#include "tgx/tgx.h"

#include <cstdio>
#include <print>

int main() {
    auto app = tgx::App::create({.title = "tgx - 03 texture"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    // An 8x8 checkerboard; pixel (0, 0) is the top-left one.
    auto image = tgx::Image::create({8, 8});
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            image.at(x, y) = (x + y) % 2 == 0 ? tgx::colors::white : tgx::colors::gray;
        }
    }
    image.at(0, 0) = tgx::colors::red;

    // Nearest filtering keeps the pixels sharp squares when drawn big.
    auto texture = tgx::Texture::create(image, {.filter = tgx::TextureFilter::nearest});
    if (!texture) {
        std::println(stderr, "texture: {}", texture.error());
        return 1;
    }

    auto &canvas = app->canvas();

    while (!app->should_close()) {
        app->poll_events();

        canvas.clear(tgx::colors::dark_gray);

        // At its own size: 8x8 units, top-left corner at the position.
        canvas.sprite(*texture, {{100, 100}});

        // Stretched to 320x320.
        canvas.sprite(*texture, {.position = {200, 100}, .size = {320, 320}});

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
