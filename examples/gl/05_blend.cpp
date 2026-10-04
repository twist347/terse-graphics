// Render state, set per draw: here blending. The same two overlapping
// see-through squares, drawn on the left with Blend::none, which overwrites
// and ignores alpha, and on the right with Blend::alpha, which mixes.

#include "tgx/gl.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <print>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec2 position;
    };

    // One square, moved by u_offset: no need for a buffer per square.
    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;

        uniform vec2 u_offset;

        void main() {
            gl_Position = vec4(in_position + u_offset, 0.0, 1.0);
        }
    )";

    constexpr const char *fragment_source = TGX_GLSL_VERSION R"(
        uniform vec4 u_color;

        out vec4 out_color;

        void main() {
            out_color = u_color;
        }
    )";

    constexpr std::array vertices{
        Vertex{{-0.2f, -0.3f}},
        Vertex{{0.2f, -0.3f}},
        Vertex{{0.2f, 0.3f}},
        Vertex{{-0.2f, 0.3f}},
    };

    constexpr std::array<std::uint16_t, 6> indices{
        0, 1, 2,
        0, 2, 3,
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
    };
}

int main() {
    // Info also prints which GL context the driver gave.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "gl - 05 blend"});
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

    const auto u_offset = shader->uniform<tgx::Vec2>("u_offset");
    const auto u_color = shader->uniform<tgx::Color>("u_color");

    auto vbo = tgx::gl::Buffer<Vertex>::create(vertices);
    auto ibo = tgx::gl::Buffer<std::uint16_t>::create(indices);
    if (!vbo || !ibo) {
        std::println(stderr, "buffer: {}", !vbo ? vbo.error() : ibo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(layout);
    vao.set_vertex_buffer(*vbo);
    vao.set_index_buffer(*ibo);

    // A red square, then a blue one over its corner, both half transparent.
    const auto squares = [&](float x, tgx::Blend blend) {
        const tgx::gl::DrawParams params{.state = {.blend = blend}};

        shader->set(u_offset, {x, 0.1f});
        shader->set(u_color, tgx::colors::red.with_alpha(128));
        app->device().draw(*shader, vao, params);

        shader->set(u_offset, {x + 0.2f, -0.1f});
        shader->set(u_color, tgx::colors::blue.with_alpha(128));
        app->device().draw(*shader, vao, params);
    };

    while (!app->should_close()) {
        app->poll_events();

        app->device().clear({.color = tgx::colors::light_gray});
        squares(-0.6f, tgx::Blend::none);
        squares(0.2f, tgx::Blend::alpha);

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
