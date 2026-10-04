// The way from the canvas down to GL: a canvas drawing through a shader of
// your own. Left, the shapes as they are; right, the same shapes through a
// shader that turns them gray.

#include "tgx/gl.h"

#include <cstdio>
#include <print>
#include <string>

namespace {
    // Takes what the canvas's own shader takes; see Canvas::set_shader.
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

        out vec4 out_color;

        void main() {
            vec4 c = texture(u_texture, uv) * color;
            float gray = dot(c.rgb, vec3(0.299, 0.587, 0.114));
            out_color = vec4(vec3(gray), c.a);
        }
    )";

    auto draw_shapes(tgx::Canvas &canvas, float x) noexcept -> void {
        canvas.rect({x, 200, 200, 140}, tgx::colors::red);
        canvas.circle({x + 340, 270}, 80, tgx::colors::yellow);
        canvas.rect({x, 400, 200, 140}, tgx::colors::green);
        canvas.circle({x + 340, 470}, 80, tgx::colors::blue);
    }
}

int main() {
    // Info also prints which GL context the driver gave.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "gl - 07 canvas shader"});
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

    // The canvas as it is, and a copy that draws through our shader.
    auto &canvas = app->canvas();
    tgx::Canvas gray = canvas;
    gray.set_shader(&*shader);

    while (!app->should_close()) {
        app->poll_events();

        canvas.clear(tgx::colors::dark_gray);
        draw_shapes(canvas, 100);
        draw_shapes(gray, 700);

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
