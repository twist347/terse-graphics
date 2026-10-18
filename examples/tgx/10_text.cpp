// Text in the built-in font: sizes, colors, lines, centering by its measured
// size, and a field that takes what is typed.

#include "tgx/tgx.hpp"

#include <cstdio>
#include <print>
#include <string>

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 10 text"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    const auto &input = app->input();
    auto &canvas = app->canvas();
    std::string typed{};

    while (!app->should_close()) {
        app->poll_events();

        // What was typed this frame, as the keyboard layout makes it.
        typed += input.text();
        // One character back, again and again while held: UTF-8
        // continuation bytes are 10xxxxxx.
        if (input.repeated(tgx::Key::backspace)) {
            while (!typed.empty() && (static_cast<unsigned char>(typed.back()) & 0xc0) == 0x80) {
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

        // anchor: which point of the text goes to the position. Centered on
        // the window, each line by its own width; right-aligned to an edge.
        const tgx::Size size = canvas.size();
        const auto width = static_cast<float>(size.width);
        canvas.text({width / 2, 340}, "Centered\nline by line", tgx::colors::white, 32, {0.5f, 0.5f});
        canvas.text({width - 40, 200}, "Right-aligned\n42", tgx::colors::light_gray, 16, {1, 0});

        // A field: a box, what was typed, a cursor after it.
        constexpr tgx::Rect field{40, 440, 600, 40};
        const tgx::Vec2 typed_extent = tgx::Canvas::measure_text(typed, 32);
        canvas.rect(field, tgx::colors::black);
        canvas.rect_lines(field, tgx::colors::gray, 2);
        canvas.text({field.x + 8, field.y + 4}, typed, tgx::colors::white, 32);
        canvas.rect({field.x + 8 + typed_extent.x + 2, field.y + 6, 2, 28}, tgx::colors::yellow);
        canvas.text({40, 490}, "Type here; Backspace erases.", tgx::colors::gray);

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
