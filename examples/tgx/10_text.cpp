// Text in the built-in font: sizes, colors, lines, centering by its measured
// size, and a field that takes what is typed.

#include "tgx/tgx.h"

#include <cstdio>
#include <format>
#include <print>
#include <string>

int main() {
    auto app = tgx::App::create({.title = "tgx - 10 text"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    const auto &input = app->input();
    auto &canvas = app->canvas();
    std::string typed;

    while (!app->should_close()) {
        app->poll_events();

        // What was typed this frame, as the keyboard layout makes it.
        typed += input.text();
        // One character back: UTF-8 continuation bytes are 10xxxxxx.
        if (input.pressed(tgx::Key::backspace)) {
            while (!typed.empty() && (static_cast<unsigned char>(typed.back()) & 0xC0) == 0x80) {
                typed.pop_back();
            }
            if (!typed.empty()) {
                typed.pop_back();
            }
        }

        canvas.clear(tgx::colors::dark_gray);

        // The font's own size is 16; whole multiples keep it sharp.
        canvas.text({40, 40}, "16: The quick brown fox jumps over the lazy dog.", tgx::colors::white);
        canvas.text({40, 70}, "32: The quick brown fox", tgx::colors::yellow, 32);
        canvas.text({40, 120}, "48: tgx", tgx::colors::cyan, 48);

        // '\n' starts a new line under the first.
        canvas.text({40, 200}, "Several lines,\none under another.", tgx::colors::light_gray);

        // Numbers, as a HUD shows them.
        canvas.text({40, 260}, std::format("fps: {:.0f}", app->clock().fps()), tgx::colors::green);

        // Centered on the window by its measured size.
        const tgx::Size size = canvas.size();
        const char *title = "Centered";
        const tgx::Vec2 extent = tgx::Canvas::measure_text(title, 32);
        canvas.text(
            {(static_cast<float>(size.width) - extent.x) / 2, 340},
            title,
            tgx::colors::white,
            32
        );

        // A field: a box, what was typed, a cursor after it.
        const tgx::Rect field{40, 440, 600, 40};
        const tgx::Vec2 typed_extent = tgx::Canvas::measure_text(typed, 32);
        canvas.rect(field, tgx::colors::black);
        canvas.rect_lines(field, tgx::colors::gray, 2);
        canvas.text({field.x + 8, field.y + 4}, typed, tgx::colors::white, 32);
        canvas.rect({field.x + 8 + typed_extent.x + 2, field.y + 6, 2, 28}, tgx::colors::yellow);
        canvas.text({40, 490}, "Type here; Backspace erases.", tgx::colors::gray);

        app->swap_buffers();
    }

    return 0;
}
