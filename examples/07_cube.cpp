#include "tgx/tgx.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <print>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec3 position;
        tgx::Color color;
    };

    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec3 in_position;
        layout(location = 1) in vec4 in_color;

        uniform mat4 u_mvp;

        out vec4 color;

        void main() {
            gl_Position = u_mvp * vec4(in_position, 1.0);
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

    // A face of the unit cube: its outward normal, two edge directions with
    // cross(u, v) == normal, and its color.
    struct Face {
        tgx::Vec3 normal;
        tgx::Vec3 u;
        tgx::Vec3 v;
        tgx::Color color;
    };

    constexpr std::array faces{
        Face{{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, tgx::colors::red},
        Face{{-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, tgx::colors::cyan},
        Face{{0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}, tgx::colors::green},
        Face{{0.f, -1.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, tgx::colors::magenta},
        Face{{0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, tgx::colors::blue},
        Face{{0.f, 0.f, -1.f}, {0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}, tgx::colors::yellow},
    };

    // Four corners per face rather than eight shared ones, so each face keeps
    // its own color. With cross(u, v) pointing out, going -u-v, +u-v, +u+v,
    // -u+v runs counter-clockwise seen from outside: the front, which Cull::back
    // keeps.
    [[nodiscard]] constexpr auto make_vertices() noexcept -> std::array<Vertex, faces.size() * 4> {
        std::array<Vertex, faces.size() * 4> vertices{};
        for (std::size_t i = 0; i < faces.size(); ++i) {
            const Face &face = faces[i];
            const tgx::Vec3 center = face.normal * 0.5f;
            const tgx::Vec3 u = face.u * 0.5f;
            const tgx::Vec3 v = face.v * 0.5f;

            vertices[i * 4 + 0] = {center - u - v, face.color};
            vertices[i * 4 + 1] = {center + u - v, face.color};
            vertices[i * 4 + 2] = {center + u + v, face.color};
            vertices[i * 4 + 3] = {center - u + v, face.color};
        }
        return vertices;
    }

    // Two triangles per face, both keeping the corner order above.
    [[nodiscard]] constexpr auto make_indices() noexcept -> std::array<std::uint16_t, faces.size() * 6> {
        std::array<std::uint16_t, faces.size() * 6> indices{};
        for (std::size_t i = 0; i < faces.size(); ++i) {
            const auto first = static_cast<std::uint16_t>(i * 4);
            constexpr std::array<std::uint16_t, 6> face{0, 1, 2, 0, 2, 3};
            for (std::size_t j = 0; j < face.size(); ++j) {
                indices[i * 6 + j] = static_cast<std::uint16_t>(first + face[j]);
            }
        }
        return indices;
    }

    constexpr auto vertices = make_vertices();
    constexpr auto indices = make_indices();

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
        tgx::gl::VertexAttribute::of(1, &Vertex::color),
    };

    // Opaque and closed: the nearest face wins, and the faces turned away are
    // never seen, so they are not drawn at all.
    constexpr tgx::RenderState solid{
        .depth = tgx::Depth::less,
        .cull = tgx::Cull::back,
    };

    [[nodiscard]] auto projection_for(tgx::Size size) noexcept -> tgx::Mat4 {
        return tgx::perspective(tgx::radians(60.f), size.aspect(), 0.1f, 100.f);
    }

    constexpr const char *title = "tgx - 07 cube";
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

    const auto u_mvp = shader->uniform<tgx::Mat4>("u_mvp");

    auto vbo = tgx::gl::Buffer::create(app->device(), vertices);
    auto ibo = tgx::gl::Buffer::create(app->device(), indices);
    if (!vbo || !ibo) {
        std::println(stderr, "buffer: {}", !vbo ? vbo.error() : ibo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(app->device(), layout);
    vao.set_vertex_buffer(*vbo);
    vao.set_index_buffer(*ibo, tgx::gl::IndexType::uint16);

    app->device().set_clear_color(tgx::colors::dark_gray);

    const tgx::Mat4 view = tgx::look_at({0.f, 1.5f, 3.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
    tgx::Mat4 projection = projection_for(app->window().framebuffer_size());

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        if (app->resized()) {
            projection = projection_for(app->window().framebuffer_size());
        }

        const auto t = static_cast<float>(app->clock().elapsed());
        const tgx::Mat4 model = tgx::rotate(t, {0.f, 1.f, 0.f}) * tgx::rotate(t * 0.6f, {1.f, 0.f, 0.f});
        shader->set(u_mvp, projection * view * model);

        app->device().clear(tgx::ClearMask::color | tgx::ClearMask::depth);
        app->device().draw(*shader, vao, {.state = solid});

        app->swap_buffers();
    }

    return 0;
}
