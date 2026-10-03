#include "tgx/gl.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <print>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec2 position;
    };

    // One unit square, placed and colored per draw by uniforms.
    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;

        uniform mat4 u_projection;
        uniform vec2 u_offset;
        uniform vec2 u_size;

        void main() {
            gl_Position = u_projection * vec4(in_position * u_size + u_offset, 0.0, 1.0);
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
        Vertex{{0.f, 0.f}},
        Vertex{{1.f, 0.f}},
        Vertex{{1.f, 1.f}},
        Vertex{{0.f, 1.f}},
    };

    constexpr std::array<std::uint16_t, 6> indices{
        0, 1, 2,
        0, 2, 3,
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
    };

    // One column each, left to right.
    constexpr std::array modes{
        tgx::Blend::none,
        tgx::Blend::alpha,
        tgx::Blend::premultiplied,
        tgx::Blend::additive,
        tgx::Blend::multiply,
    };

    // The scene is laid out in a 16 x 9 box, the default window's shape.
    constexpr float column_width = 16.f / static_cast<float>(modes.size());

    constexpr tgx::Color band = tgx::Color::rgb(0xE8D8B0);
    constexpr tgx::Color red = tgx::colors::red.with_alpha(160);
    constexpr tgx::Color blue = tgx::Color::rgb(0x3070FF).with_alpha(160);

    constexpr const char *title = "tgx - 06 blend: none | alpha | premultiplied | additive | multiply";
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

    const auto u_projection = shader->uniform<tgx::Mat4>("u_projection");
    const auto u_offset = shader->uniform<tgx::Vec2>("u_offset");
    const auto u_size = shader->uniform<tgx::Vec2>("u_size");
    const auto u_color = shader->uniform<tgx::Color>("u_color");

    shader->set(u_projection, tgx::ortho(0.f, 16.f, 0.f, 9.f));

    auto vbo = tgx::gl::Buffer::create(vertices);
    auto ibo = tgx::gl::Buffer::create(indices);
    if (!vbo || !ibo) {
        std::println(stderr, "buffer: {}", !vbo ? vbo.error() : ibo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(layout);
    vao.set_vertex_buffer(*vbo);
    vao.set_index_buffer(*ibo, tgx::gl::IndexType::uint16);

    const auto rect = [&](tgx::Vec2 offset, tgx::Vec2 size, tgx::Color color, tgx::Blend blend) {
        shader->set(u_offset, offset);
        shader->set(u_size, size);
        shader->set(u_color, color);
        app->device().draw(*shader, vao, {.state = {.blend = blend}});
    };

    while (!app->should_close()) {
        app->poll_events();

        app->device().clear({.color = tgx::colors::dark_gray});

        // A light band across the middle, so each mode shows both over the
        // dark background and over something bright.
        rect({0.f, 3.5f}, {16.f, 2.f}, band, tgx::Blend::none);

        for (std::size_t i = 0; i < modes.size(); ++i) {
            const tgx::Blend mode = modes[i];
            const float x = static_cast<float>(i) * column_width;
            // These two modes take colors already multiplied by their alpha.
            const bool premultiplied = mode == tgx::Blend::premultiplied || mode == tgx::Blend::multiply;

            // Two see-through squares that overlap each other and the band.
            rect({x + 0.4f, 2.2f}, {1.8f, 3.2f}, premultiplied ? red.premultiplied() : red, mode);
            rect({x + 1.2f, 3.6f}, {1.8f, 3.2f}, premultiplied ? blue.premultiplied() : blue, mode);
        }

        app->swap_buffers();
    }

    return 0;
}
