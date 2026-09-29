#include "tgx/tgx.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <numbers>
#include <string>

namespace {
    struct Vertex {
        float x;
        float y;
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

    // The rim is split into this many segments; more makes it rounder.
    constexpr std::size_t segments = 64;
    constexpr float radius = 0.7f;

    constexpr std::array layout{
        tgx::gl::VertexAttribute{
            .location = 0,
            .format = tgx::gl::VertexFormat::float32x2,
            .offset = offsetof(Vertex, x),
        },
        tgx::gl::VertexAttribute{
            .location = 1,
            .format = tgx::gl::VertexFormat::unorm8x4,
            .offset = offsetof(Vertex, color),
        },
    };

    constexpr const char *title = "tgx - 04 circle";

    // Hue in [0, 1) to a fully saturated color, so the rim runs through the
    // rainbow.
    [[nodiscard]] auto hue(float h) noexcept -> tgx::Color {
        const auto channel = [h](float offset) {
            const float k = std::fmod(h * 6.0f + offset, 6.0f);
            const float v = 1.0f - std::max(0.0f, std::min({k, 4.0f - k, 1.0f}));
            return static_cast<std::uint8_t>(std::lround(v * 255.0f));
        };
        return {channel(5.0f), channel(3.0f), channel(1.0f), 255};
    }

    // Vertex 0 is the center, then the rim. Positions are in clip space, which
    // stretches with the window, so x is squeezed by the aspect ratio to keep
    // the circle round.
    [[nodiscard]] auto make_vertices(tgx::Size size) noexcept -> std::array<Vertex, segments + 1> {
        const float aspect = size.width > 0 && size.height > 0
            ? static_cast<float>(size.height) / static_cast<float>(size.width)
            : 1.0f;

        std::array<Vertex, segments + 1> vertices{};
        vertices[0] = {0.0f, 0.0f, tgx::colors::white};
        for (std::size_t i = 0; i < segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float angle = t * 2.0f * std::numbers::pi_v<float>;
            vertices[i + 1] = {radius * aspect * std::cos(angle), radius * std::sin(angle), hue(t)};
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
}

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = title});
    if (!app) {
        std::fprintf(stderr, "app: %s\n", tgx::to_str(app.error()));
        return 1;
    }

    std::string log;
    auto shader = tgx::gl::Shader::from_source(app->device(), vertex_source, fragment_source, &log);
    if (!shader) {
        std::fprintf(stderr, "shader: %s\n%s", tgx::to_str(shader.error()), log.c_str());
        return 1;
    }

    // Dynamic: the vertices are rebuilt whenever the window changes shape.
    auto vbo = tgx::gl::Buffer::create(
        app->device(),
        make_vertices(app->window().framebuffer_size()),
        tgx::gl::BufferAccess::dynamic
    );
    auto ibo = tgx::gl::Buffer::create(app->device(), indices);
    if (!vbo || !ibo) {
        std::fprintf(stderr, "buffer: %s\n", tgx::to_str(!vbo ? vbo.error() : ibo.error()));
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
            vbo->update(0, make_vertices(app->window().framebuffer_size()));
        }

        app->device().clear();
        app->device().draw(*shader, vao, {.count = indices.size()});

        app->swap_buffers();
    }

    return 0;
}
