// Uniforms: values the shader reads that are the same for every vertex of a
// draw. The color is set once; the transform every frame, turning the
// triangle without touching its vertices.

#include "tgx/gl.h"

#include <array>
#include <cstdio>
#include <numbers>
#include <print>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec2 position;
    };

    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;

        uniform mat4 u_transform;

        void main() {
            gl_Position = u_transform * vec4(in_position, 0.0, 1.0);
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
        Vertex{{-0.4f, -0.3f}},
        Vertex{{0.4f, -0.3f}},
        Vertex{{0.0f, 0.5f}},
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
    };
}

int main() {
    auto app = tgx::App::create({.title = "gl - 03 uniforms"});
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

    // Looked up by name once, with the type they have in the shader; after
    // that they are set without names.
    const auto u_transform = shader->uniform<tgx::Mat4>("u_transform");
    const auto u_color = shader->uniform<tgx::Color>("u_color");

    // A shader keeps its uniforms until they are set again.
    shader->set(u_color, tgx::colors::yellow);

    auto vbo = tgx::gl::Buffer::create(vertices);
    if (!vbo) {
        std::println(stderr, "buffer: {}", vbo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(layout);
    vao.set_vertex_buffer(*vbo);

    while (!app->should_close()) {
        app->poll_events();

        // Half a turn a second, about the z axis, which points out of the
        // screen. Clip space is stretched over the whole window, so x is then
        // squeezed by its aspect ratio to keep the triangle's shape.
        const auto angle = static_cast<float>(app->clock().elapsed()) * std::numbers::pi_v<float>;
        const float aspect = app->window().framebuffer_size().aspect();
        shader->set(u_transform, tgx::scale({1 / aspect, 1, 1}) * tgx::rotate(angle, {0, 0, 1}));

        app->device().clear({.color = tgx::colors::dark_gray});
        app->device().draw(*shader, vao);

        app->swap_buffers();
    }

    return 0;
}
