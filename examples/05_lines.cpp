#include "tgx/tgx.h"

#include <array>
#include <cmath>
#include <cstddef>
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

    constexpr const char *title = "tgx - 05 lines";

    // Grid lines every 0.25 across clip space, both ways: 9 of each.
    constexpr std::size_t grid_steps = 9;
    // Primitive::lines takes vertices in pairs, one pair per segment.
    constexpr std::size_t grid_vertices = grid_steps * 2 * 2;
    // Primitive::line_strip joins every vertex to the next one.
    constexpr std::size_t wave_vertices = 256;

    // Both shapes share one buffer: the grid first, then the wave. Each is
    // drawn on its own with DrawParams::first and count.
    using Vertices = std::array<Vertex, grid_vertices + wave_vertices>;

    constexpr tgx::Color grid_color{80, 80, 80, 255};
    constexpr tgx::Color axis_color{160, 160, 160, 255};

    [[nodiscard]] constexpr auto make_grid() noexcept -> std::array<Vertex, grid_vertices> {
        std::array<Vertex, grid_vertices> grid{};
        std::size_t n = 0;
        for (std::size_t i = 0; i < grid_steps; ++i) {
            const float at = -1.0f + static_cast<float>(i) * 0.25f;
            // The middle line of each direction is an axis.
            const tgx::Color color = i == grid_steps / 2 ? axis_color : grid_color;
            grid[n++] = {at, -1.0f, color};
            grid[n++] = {at, 1.0f, color};
            grid[n++] = {-1.0f, at, color};
            grid[n++] = {1.0f, at, color};
        }
        return grid;
    }

    // A sine wave across the window, sliding to the left as time goes on.
    [[nodiscard]] auto make_wave(double time) noexcept -> std::array<Vertex, wave_vertices> {
        std::array<Vertex, wave_vertices> wave{};
        for (std::size_t i = 0; i < wave_vertices; ++i) {
            const float x = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(wave_vertices - 1);
            const auto phase = static_cast<float>(std::fmod(time * 2.0, 2.0 * std::numbers::pi));
            wave[i] = {x, 0.5f * std::sin(x * 3.0f * std::numbers::pi_v<float> + phase), tgx::colors::cyan};
        }
        return wave;
    }
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

    // Sized for everything, filled piecewise: the grid once, the wave every
    // frame.
    auto vbo = tgx::gl::Buffer::create(app->device(), sizeof(Vertices), tgx::gl::BufferAccess::dynamic);
    if (!vbo) {
        std::fprintf(stderr, "buffer: %s\n", tgx::to_str(vbo.error()));
        return 1;
    }
    vbo->update(0, make_grid());

    auto vao = tgx::gl::VertexArray::create<Vertex>(app->device(), layout);
    vao.set_vertex_buffer(*vbo);

    app->device().set_clear_color(tgx::colors::black);

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        // Only the wave moves: rewrite just its part of the buffer.
        vbo->update(grid_vertices * sizeof(Vertex), make_wave(app->clock().elapsed()));

        app->device().clear();
        app->device().draw(*shader, vao, {
            .count = grid_vertices,
            .primitive = tgx::Primitive::lines,
        });
        app->device().draw(*shader, vao, {
            .count = wave_vertices,
            .first = grid_vertices,
            .primitive = tgx::Primitive::line_strip,
        });

        app->swap_buffers();
    }

    return 0;
}
