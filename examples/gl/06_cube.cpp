// 3D: a turning cube seen in perspective. The depth test keeps the nearest
// face in front, and culling skips the faces turned away from the camera.

#include "tgx/gl.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <print>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec3 position;
        tgx::Color color;
    };

    // u_mvp takes a vertex from the cube's own space to clip space in one go:
    // model (where the cube is), view (where the camera is), projection (how
    // the camera sees).
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

    // Four corners per face, so each face has its own color. Seen from
    // outside, each face's corners go counter-clockwise: that is its front.
    constexpr std::array vertices{
        // +x
        Vertex{{ 0.5f, -0.5f, -0.5f}, tgx::colors::red},
        Vertex{{ 0.5f,  0.5f, -0.5f}, tgx::colors::red},
        Vertex{{ 0.5f,  0.5f,  0.5f}, tgx::colors::red},
        Vertex{{ 0.5f, -0.5f,  0.5f}, tgx::colors::red},
        // -x
        Vertex{{-0.5f, -0.5f, -0.5f}, tgx::colors::cyan},
        Vertex{{-0.5f, -0.5f,  0.5f}, tgx::colors::cyan},
        Vertex{{-0.5f,  0.5f,  0.5f}, tgx::colors::cyan},
        Vertex{{-0.5f,  0.5f, -0.5f}, tgx::colors::cyan},
        // +y
        Vertex{{-0.5f,  0.5f, -0.5f}, tgx::colors::green},
        Vertex{{-0.5f,  0.5f,  0.5f}, tgx::colors::green},
        Vertex{{ 0.5f,  0.5f,  0.5f}, tgx::colors::green},
        Vertex{{ 0.5f,  0.5f, -0.5f}, tgx::colors::green},
        // -y
        Vertex{{-0.5f, -0.5f, -0.5f}, tgx::colors::magenta},
        Vertex{{ 0.5f, -0.5f, -0.5f}, tgx::colors::magenta},
        Vertex{{ 0.5f, -0.5f,  0.5f}, tgx::colors::magenta},
        Vertex{{-0.5f, -0.5f,  0.5f}, tgx::colors::magenta},
        // +z
        Vertex{{-0.5f, -0.5f,  0.5f}, tgx::colors::blue},
        Vertex{{ 0.5f, -0.5f,  0.5f}, tgx::colors::blue},
        Vertex{{ 0.5f,  0.5f,  0.5f}, tgx::colors::blue},
        Vertex{{-0.5f,  0.5f,  0.5f}, tgx::colors::blue},
        // -z
        Vertex{{-0.5f, -0.5f, -0.5f}, tgx::colors::yellow},
        Vertex{{-0.5f,  0.5f, -0.5f}, tgx::colors::yellow},
        Vertex{{ 0.5f,  0.5f, -0.5f}, tgx::colors::yellow},
        Vertex{{ 0.5f, -0.5f, -0.5f}, tgx::colors::yellow},
    };

    // Two triangles per face, keeping the corners' order.
    constexpr std::array<std::uint16_t, 36> indices{
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
        tgx::gl::VertexAttribute::of(1, &Vertex::color),
    };

    // Nearer faces win, and back faces are not drawn at all.
    constexpr tgx::gl::RenderState solid{
        .depth = tgx::gl::Depth::less,
        .cull = tgx::gl::Cull::back,
    };
}

int main() {
    auto app = tgx::App::create({.title = "gl - 06 cube"});
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

    const auto u_mvp = shader->uniform<tgx::Mat4>("u_mvp");

    auto vbo = tgx::gl::Buffer::create(vertices);
    auto ibo = tgx::gl::Buffer::create(indices);
    if (!vbo || !ibo) {
        std::println(stderr, "buffer: {}", !vbo ? vbo.error() : ibo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(layout);
    vao.set_vertex_buffer(*vbo);
    vao.set_index_buffer(*ibo, tgx::gl::IndexType::uint16);

    // The camera: up and back from the cube, looking at its middle.
    const tgx::Mat4 view = tgx::look_at({0, 1.5f, 3}, {0, 0, 0}, {0, 1, 0});

    while (!app->should_close()) {
        app->poll_events();

        // A 60 degree field of view, for the window's shape now.
        const float aspect = app->window().framebuffer_size().aspect();
        const tgx::Mat4 projection = tgx::perspective(tgx::radians(60), aspect, 0.1f, 100);

        // Turning about the vertical axis, one radian a second.
        const auto t = static_cast<float>(app->clock().elapsed());
        const tgx::Mat4 model = tgx::rotate(t, {0, 1, 0});

        shader->set(u_mvp, projection * view * model);

        // The depth buffer is cleared too: every frame starts with nothing in
        // front.
        app->device().clear({.color = tgx::colors::dark_gray, .depth = 1});
        app->device().draw(*shader, vao, {.state = solid});

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
