// An index buffer: a rectangle from four vertices instead of six, the two
// triangles sharing two of them.

#include "tgx/tgx.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <print>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec2 position;
        tgx::Color color;
    };

    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec4 in_color;

        out vec4 color;

        void main() {
            gl_Position = vec4(in_position, 0.0, 1.0);
            color = in_color;
        }
    )";

    constexpr const char *fragment_source = TGX_GLSL_VERSION R"(
        in vec4 color;

        out vec4 out_color;

        void main() {
            out_color = color;
        }
    )";

    // The four corners, each stored once.
    constexpr std::array vertices{
        Vertex{{-0.6f, -0.5f}, tgx::colors::red},
        Vertex{{0.6f, -0.5f}, tgx::colors::green},
        Vertex{{0.6f, 0.5f}, tgx::colors::blue},
        Vertex{{-0.6f, 0.5f}, tgx::colors::yellow},
    };

    // Two triangles by vertex number, both using corners 0 and 2.
    constexpr std::array<std::uint16_t, 6> indices{
        0, 1, 2,
        0, 2, 3,
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
        tgx::gl::VertexAttribute::of(1, &Vertex::color),
    };
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "gl - 02 indexed"});
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
    auto ibo = tgx::gl::Buffer<std::uint16_t>::create(indices);
    if (!vbo || !ibo) {
        std::println(stderr, "buffer: {}", !vbo ? vbo.error() : ibo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(layout);
    vao.set_vertex_buffer(*vbo);
    // With an index buffer attached, draws go by the indices.
    vao.set_index_buffer(*ibo);

    while (!app->should_close()) {
        app->poll_events();

        app->device().clear({.color = tgx::colors::dark_gray});
        app->device().draw(*shader, vao);

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
