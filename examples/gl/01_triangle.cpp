// The GL level from the bottom: vertices in a buffer, a vertex array that says
// how to read them, a shader that turns them into pixels, and a draw.

#include "tgx/gl.h"

#include <array>
#include <cstdio>
#include <print>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec2 position;
        tgx::Color color;
    };

    // Positions go straight through: GL's clip space, (-1, -1) at the
    // bottom-left of the window, (1, 1) at the top-right.
    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec4 in_color;

        out vec4 color;

        void main() {
            gl_Position = vec4(in_position, 0.0, 1.0);
            color = in_color;
        }
    )";

    // The color is blended across the triangle between its corners.
    constexpr const char *fragment_source = TGX_GLSL_VERSION R"(
        in vec4 color;

        out vec4 out_color;

        void main() {
            out_color = color;
        }
    )";

    constexpr std::array vertices{
        Vertex{{-0.6f, -0.5f}, tgx::colors::red},
        Vertex{{0.6f, -0.5f}, tgx::colors::green},
        Vertex{{0.f, 0.6f}, tgx::colors::blue},
    };

    // Which field feeds which `layout(location = N)` input of the shader.
    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
        tgx::gl::VertexAttribute::of(1, &Vertex::color),
    };
}

int main() {
    // Info also prints which GL context the driver gave.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "gl - 01 triangle"});
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

    auto vbo = tgx::gl::Buffer<Vertex>::create(vertices);
    if (!vbo) {
        std::println(stderr, "buffer: {}", vbo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(layout);
    vao.set_vertex_buffer(*vbo);

    while (!app->should_close()) {
        app->poll_events();

        app->device().clear({.color = tgx::colors::dark_gray});
        app->device().draw(*shader, vao);

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
