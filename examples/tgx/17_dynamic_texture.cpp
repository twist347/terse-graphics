// A dynamic texture painted with the mouse. The Image brushes live on the CPU;
// Texture::update copies one to the GPU. Left button paints, right erases,
// C clears. Nearest filtering keeps the pixels sharp at any window size.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <cstdio>
#include <print>

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 17 dynamic texture"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &canvas = app->canvas();
    canvas.set_size({1280, 720});
    const auto &input = app->input();

    constexpr tgx::Size texture_size{320, 180};
    constexpr tgx::Rect texture_area{0, 0, 320, 180};
    constexpr int brush_side = 5;

    // create(Size) always makes a dynamic texture; its initial contents are
    // unspecified, so fill it through update before the first draw.
    auto texture = tgx::Texture::create(texture_size, {.filter = tgx::TextureFilter::nearest});
    if (!texture) {
        std::println(stderr, "texture: {}", texture.error());
        return 1;
    }
    const tgx::Image background = tgx::Image::create(texture_size, {16, 20, 32, 255});
    const tgx::Image paint = tgx::Image::create({brush_side, brush_side}, tgx::colors::yellow);
    const tgx::Image erase = tgx::Image::create({brush_side, brush_side}, {16, 20, 32, 255});
    texture->update(0, 0, background);

    while (!app->should_close()) {
        app->poll_events();
        if (canvas.size().empty()) {
            app->swap_buffers();
            continue;
        }

        if (input.pressed(tgx::Key::c)) {
            texture->update(0, 0, background);
        }

        // Mouse: window -> the letterboxed Canvas -> texture pixels.
        const tgx::Vec2 canvas_mouse = canvas.to_world(input.mouse());
        const tgx::Rect canvas_area{0, 0, tgx::to_vec2(canvas.size()).x, tgx::to_vec2(canvas.size()).y};
        const tgx::Vec2 at = tgx::remap(canvas_mouse, canvas_area, texture_area);
        const bool painting = input.down(tgx::MouseButton::left);
        const bool erasing = input.down(tgx::MouseButton::right);
        if ((painting || erasing) && tgx::contains(texture_area, at)) {
            // Keep the whole brush inside the texture at the edges.
            const int x = std::clamp(static_cast<int>(at.x) - brush_side / 2, 0, texture_size.width - brush_side);
            const int y = std::clamp(static_cast<int>(at.y) - brush_side / 2, 0, texture_size.height - brush_side);
            texture->update(x, y, painting ? paint : erase);
        }

        canvas.clear(tgx::colors::black);
        canvas.sprite(*texture, {.size = tgx::to_vec2(canvas.size())});

        canvas.rect({0, 0, tgx::to_vec2(canvas.size()).x, 48}, tgx::colors::black.with_alpha(200));
        canvas.text({16, 16}, "Left: paint   Right: erase   C: clear", tgx::colors::white);
        canvas.fps({16, 62});

        app->swap_buffers();
    }

    return 0;
}
