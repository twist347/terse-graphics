#include "tgx/tgx.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

namespace {
    struct Vertex {
        float x;
        float y;
        tgx::Color color;
    };

    constexpr const char *vertex_source = R"(
        #version 450 core
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec4 in_color;

        out vec4 color;

        void main() {
            gl_Position = vec4(in_position, 0.0, 1.0);
            color = in_color;
        }
    )";

    constexpr const char *fragment_source = R"(
        #version 450 core
        in vec4 color;

        out vec4 out_color;

        void main() {
            out_color = color;
        }
    )";

    constexpr std::array vertices{
        Vertex{-0.6f, -0.5f, tgx::colors::red},
        Vertex{0.6f, -0.5f, tgx::colors::green},
        Vertex{0.0f, 0.6f, tgx::colors::blue},
    };

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

    constexpr const char *title = "tgx - 02 triangle";
}

int main() {
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

    auto vbo = tgx::gl::Buffer::create(app->device(), std::span{vertices});
    if (!vbo) {
        std::fprintf(stderr, "buffer: %s\n", tgx::to_str(vbo.error()));
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create(app->device(), sizeof(Vertex), layout);
    vao.set_vertex_buffer(*vbo);

    app->device().set_clear_color(tgx::colors::dark_gray);

    while (app->next_frame()) {
        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        app->device().clear();
        app->device().draw(*shader, vao, {.count = vertices.size()});
    }

    return 0;
}
