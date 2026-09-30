#include "tgx/tgx.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <format>
#include <print>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec2 position;
        tgx::Color color;
    };

    // The vertices never change: the offset, scale and tint arrive as uniforms
    // every frame and move the triangle on the GPU.
    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec4 in_color;

        uniform vec2 u_offset;
        uniform float u_scale;

        out vec4 color;

        void main() {
            gl_Position = vec4(in_position * u_scale + u_offset, 0.0, 1.0);
            color = in_color;
        }
    )";

    constexpr const char *fragment_source = TGX_GLSL_VERSION R"(
        in vec4 color;

        uniform vec4 u_tint;

        out vec4 out_color;

        void main() {
            out_color = color * u_tint;
        }
    )";

    constexpr std::array vertices{
        Vertex{{-0.3f, -0.25f}, tgx::colors::red},
        Vertex{{0.3f, -0.25f}, tgx::colors::green},
        Vertex{{0.0f, 0.3f}, tgx::colors::blue},
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
        tgx::gl::VertexAttribute::of(1, &Vertex::color),
    };

    // Stepped through once a second.
    constexpr std::array tints{
        tgx::colors::white,
        tgx::colors::yellow,
        tgx::colors::cyan,
        tgx::colors::magenta,
    };

    constexpr float orbit_radius = 0.4f;

    constexpr const char *title = "tgx - 05 uniforms";
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

    // Looked up once; in the loop they are set without any names.
    const auto u_offset = shader->uniform<tgx::Vec2>("u_offset");
    const auto u_scale = shader->uniform<float>("u_scale");
    const auto u_tint = shader->uniform<tgx::Color>("u_tint");

    auto vbo = tgx::gl::Buffer::create(app->device(), vertices);
    if (!vbo) {
        std::println(stderr, "buffer: {}", vbo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(app->device(), layout);
    vao.set_vertex_buffer(*vbo);

    app->device().set_clear_color(tgx::colors::dark_gray);

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        const auto t = static_cast<float>(app->clock().elapsed());
        const auto second = static_cast<std::size_t>(app->clock().elapsed());

        shader->set(u_offset, {orbit_radius * std::cos(t), orbit_radius * std::sin(t)});
        shader->set(u_scale, 1.f + 0.3f * std::sin(t * 3.f));
        shader->set(u_tint, tints[second % tints.size()]);

        app->device().clear();
        app->device().draw(*shader, vao);

        app->swap_buffers();
    }

    return 0;
}
