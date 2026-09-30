#include "tgx/tgx.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <print>
#include <numbers>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec2 position;
        tgx::Color color;
    };

    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec4 in_color;

        uniform mat4 u_projection;

        out vec4 color;

        void main() {
            gl_Position = u_projection * vec4(in_position, 0.0, 1.0);
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

    // The rim is split into this many segments; more makes it rounder.
    constexpr std::size_t segments = 64;
    constexpr float radius = 0.7f;

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
        tgx::gl::VertexAttribute::of(1, &Vertex::color),
    };

    constexpr const char *title = "tgx - 04 circle";

    // Hue in [0, 1) to a fully saturated color, so the rim runs through the
    // rainbow.
    [[nodiscard]] auto hue(float h) noexcept -> tgx::Color {
        const auto channel = [h](float offset) {
            const float k = std::fmod(h * 6.f + offset, 6.f);
            const float v = 1.f - std::max(0.f, std::min({k, 4.f - k, 1.f}));
            return static_cast<std::uint8_t>(std::lround(v * 255.f));
        };
        return {channel(5.f), channel(3.f), channel(1.f), 255};
    }

    // Vertex 0 is the center, then the rim. Built once: the window's shape is
    // taken care of by the projection, not by the vertices.
    [[nodiscard]] auto make_vertices() noexcept -> std::array<Vertex, segments + 1> {
        std::array<Vertex, segments + 1> vertices{};
        vertices[0] = {{0.f, 0.f}, tgx::colors::white};
        for (std::size_t i = 0; i < segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float angle = t * 2.f * std::numbers::pi_v<float>;
            vertices[i + 1] = {{radius * std::cos(angle), radius * std::sin(angle)}, hue(t)};
        }
        return vertices;
    }

    // One triangle per segment: center, this rim vertex, the next one. The
    // last segment wraps around to rim vertex 1.
    [[nodiscard]] constexpr auto make_indices() noexcept -> std::array<std::uint16_t, segments * 3> {
        std::array<std::uint16_t, segments * 3> indices{};
        for (std::size_t i = 0; i < segments; ++i) {
            indices[i * 3 + 0] = 0;
            indices[i * 3 + 1] = static_cast<std::uint16_t>(i + 1);
            indices[i * 3 + 2] = static_cast<std::uint16_t>((i + 1) % segments + 1);
        }
        return indices;
    }

    constexpr auto indices = make_indices();

    // Clip space spans -1..1 across the window whatever its shape, which would
    // stretch the circle. This view is 2 units tall and as wide as the window's
    // aspect ratio, so a unit is equally long both ways.
    [[nodiscard]] constexpr auto projection_for(tgx::Size size) noexcept -> tgx::Mat4 {
        const float aspect = size.aspect();
        return tgx::ortho(-aspect, aspect, -1.f, 1.f);
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
    auto shader = tgx::gl::Shader::from_source(app->device(), vertex_source, fragment_source, &log);
    if (!shader) {
        std::print(stderr, "shader: {}\n{}", shader.error(), log);
        return 1;
    }

    const auto u_projection = shader->uniform<tgx::Mat4>("u_projection");
    shader->set(u_projection, projection_for(app->window().framebuffer_size()));

    auto vbo = tgx::gl::Buffer::create(app->device(), make_vertices());
    auto ibo = tgx::gl::Buffer::create(app->device(), indices);
    if (!vbo || !ibo) {
        std::println(stderr, "buffer: {}", !vbo ? vbo.error() : ibo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(app->device(), layout);
    vao.set_vertex_buffer(*vbo);
    vao.set_index_buffer(*ibo, tgx::gl::IndexType::uint16);

    app->device().set_clear_color(tgx::colors::dark_gray);

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        if (app->resized()) {
            shader->set(u_projection, projection_for(app->window().framebuffer_size()));
        }

        app->device().clear();
        app->device().draw(*shader, vao);

        app->swap_buffers();
    }

    return 0;
}
