// The mouse cursor. Over a box it takes the box's shape. Keys 1 to 3 switch
// the mode: normal, hidden over the window, captured for mouse look, where a
// sight turns with the mouse however far it moves.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <format>
#include <print>

namespace {
    struct Box {
        tgx::CursorShape shape{};
        const char *name{};
    };

    constexpr std::array boxes{
        Box{tgx::CursorShape::text, "text"},
        Box{tgx::CursorShape::hand, "hand"},
        Box{tgx::CursorShape::crosshair, "crosshair"},
        Box{tgx::CursorShape::resize_ew, "resize_ew"},
        Box{tgx::CursorShape::resize_ns, "resize_ns"},
        Box{tgx::CursorShape::resize_all, "resize_all"},
        Box{tgx::CursorShape::not_allowed, "not_allowed"},
    };

    constexpr std::array modes{
        tgx::CursorMode::normal, tgx::CursorMode::hidden, tgx::CursorMode::captured,
    };
    constexpr std::array mode_keys{tgx::Key::digit1, tgx::Key::digit2, tgx::Key::digit3};
    constexpr std::array mode_names{"normal", "hidden", "captured"};

    // Radians the sight turns per unit of mouse movement.
    constexpr float sensitivity = 0.005f;
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 14 cursor"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &window = app->window();
    const auto &input = app->input();
    auto &canvas = app->canvas();

    // Where the sight looks: yaw left-right, pitch up-down.
    float yaw = 0;
    float pitch = 0;

    while (!app->should_close()) {
        app->poll_events();

        for (std::size_t i = 0; i < modes.size(); ++i) {
            if (input.pressed(mode_keys[i])) {
                window.set_cursor_mode(modes[i]);
            }
        }

        const bool captured = window.cursor_mode() == tgx::CursorMode::captured;
        if (captured) {
            yaw += input.mouse_delta().x * sensitivity;
            pitch = std::clamp(pitch + input.mouse_delta().y * sensitivity, -1.f, 1.f);
        }

        canvas.clear(tgx::colors::dark_gray);

        // The shape of the box under the mouse, the arrow elsewhere.
        auto shape = tgx::CursorShape::arrow;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            const tgx::Rect box{40.f + 170.f * static_cast<float>(i), 120, 160, 80};
            const bool over = tgx::contains(box, input.mouse());
            if (over) {
                shape = boxes[i].shape;
            }
            canvas.rect(box, over ? tgx::colors::gray : tgx::colors::black);
            canvas.text(box.center(), boxes[i].name, tgx::colors::white, tgx::Canvas::default_text_size, {0.5f, 0.5f});
        }
        window.set_cursor_shape(shape);

        const auto mode = static_cast<std::size_t>(window.cursor_mode());
        canvas.text({40, 40}, "1: normal, 2: hidden, 3: captured (1 lets go)", tgx::colors::light_gray);
        canvas.text({40, 70}, std::format("mode: {}", mode_names[mode]), tgx::colors::white);

        // The sight: a ring that the mouse turns, its line showing which way.
        constexpr tgx::Vec2 center{640, 440};
        canvas.circle_lines(center, 120, captured ? tgx::colors::yellow : tgx::colors::gray, 2);
        canvas.line(center, center + tgx::from_angle(yaw) * 120.f, tgx::colors::yellow, 2);
        canvas.circle(center + tgx::Vec2{0, pitch * 100}, 6, tgx::colors::red);
        canvas.text(
            {640, 600},
            captured ? "move the mouse: the line turns, the dot tilts" : "press 3 to capture the mouse",
            tgx::colors::light_gray,
            tgx::Canvas::default_text_size,
            {0.5f, 0}
        );

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
