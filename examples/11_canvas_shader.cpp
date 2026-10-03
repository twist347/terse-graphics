#include "tgx/gl.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <format>
#include <print>
#include <string>

namespace {
    constexpr const char *title = "tgx - 11 canvas shader";

    // Takes what the canvas's own shader takes (see Canvas::set_shader) and
    // turns the colors gray by u_amount, from 0 (as they are) to 1 (all gray).
    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec2 in_uv;
        layout(location = 2) in vec4 in_color;

        uniform mat4 u_projection;

        out vec2 uv;
        out vec4 color;

        void main() {
            gl_Position = u_projection * vec4(in_position, 0.0, 1.0);
            uv = in_uv;
            color = in_color;
        }
    )";

    constexpr const char *fragment_source = TGX_GLSL_VERSION R"(
        in vec2 uv;
        in vec4 color;

        uniform sampler2D u_texture;
        uniform float u_amount;

        out vec4 out_color;

        void main() {
            vec4 c = texture(u_texture, uv) * color;
            float gray = dot(c.rgb, vec3(0.299, 0.587, 0.114));
            out_color = vec4(mix(c.rgb, vec3(gray), u_amount), c.a);
        }
    )";

    constexpr std::array palette{
        tgx::colors::red,
        tgx::colors::yellow,
        tgx::colors::green,
        tgx::colors::cyan,
        tgx::colors::blue,
        tgx::colors::magenta,
    };

    // The same shapes wherever they are put, to compare the two halves.
    auto draw_scene(tgx::Canvas &canvas, float x) noexcept -> void {
        for (std::size_t i = 0; i < palette.size(); ++i) {
            const float y = 80.f + 90.f * static_cast<float>(i);
            canvas.rect({x, y, 120.f, 70.f}, palette[i]);
            canvas.circle({x + 220.f, y + 35.f}, 35.f, palette[palette.size() - 1 - i]);
        }
    }
}

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = title});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    std::string log;
    auto shader = tgx::gl::Shader::from_source(vertex_source, fragment_source, &log);
    if (!shader) {
        std::print(stderr, "shader: {}\n{}", shader.error(), log);
        return 1;
    }
    const auto u_amount = shader->uniform<float>("u_amount");

    // The canvas as it is, and a copy of it that draws through our shader.
    auto &canvas = app->canvas();
    tgx::Canvas gray = canvas;
    gray.set_shader(&*shader);

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        // Fades to gray and back every few seconds.
        const auto t = static_cast<float>(app->clock().elapsed());
        shader->set(u_amount, 0.5f + 0.5f * std::sin(t));

        canvas.clear(tgx::colors::dark_gray);

        // Left: the canvas's own shader. Right: the same shapes through ours.
        draw_scene(canvas, 120.f);
        draw_scene(gray, 760.f);

        app->swap_buffers();
    }

    return 0;
}
