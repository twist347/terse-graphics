#include "tgx/tgx.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <format>
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

    // Four corners, each vertex stored once.
    constexpr std::array vertices{
        Vertex{{-0.6f, -0.5f}, tgx::colors::red},
        Vertex{{0.6f, -0.5f}, tgx::colors::green},
        Vertex{{0.6f, 0.5f}, tgx::colors::blue},
        Vertex{{-0.6f, 0.5f}, tgx::colors::yellow},
    };

    // Two triangles sharing the 0-2 diagonal: six indices instead of six
    // vertices.
    constexpr std::array<std::uint16_t, 6> indices{
        0, 1, 2,
        0, 2, 3,
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
        tgx::gl::VertexAttribute::of(1, &Vertex::color),
    };

    constexpr const char *title = "tgx - 03 rectangle";
}

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = title});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    std::string log;
    auto shader = tgx::gl::Shader::from_source(app->device(), vertex_source, fragment_source, &log);
    if (!shader) {
        std::print(stderr, "shader: {}\n{}", shader.error(), log);
        return 1;
    }

    auto vbo = tgx::gl::Buffer::create(app->device(), vertices);
    auto ibo = tgx::gl::Buffer::create(app->device(), indices);
    if (!vbo || !ibo) {
        std::println(stderr, "buffer: {}", !vbo ? vbo.error() : ibo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(app->device(), layout);
    vao.set_vertex_buffer(*vbo);
    // With an index buffer attached, draw counts indices, not vertices.
    vao.set_index_buffer(*ibo, tgx::gl::IndexType::uint16);

    app->device().set_clear_color(tgx::colors::dark_gray);

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        app->device().clear();
        app->device().draw(*shader, vao);

        app->swap_buffers();
    }

    return 0;
}
