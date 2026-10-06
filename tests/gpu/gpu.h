#pragma once

// What the GPU tests share: the one App of the run, render targets to draw
// into and read back, and a draw of their own that covers what it is drawn
// into. Most tests draw into a render target of their own: its size is theirs
// to pick, whatever the window and the display's scale are.

#include "tgx/gl.h"

#include <doctest/doctest.h>

#include <array>
#include <utility>

namespace tgx_test {
    // The one App of the run, made by main before any test.
    [[nodiscard]] auto app() -> tgx::App &;

    // Of the size, with nearest filtering so it shows the pixels drawn when
    // drawn itself.
    [[nodiscard]] inline auto target(tgx::Size size, bool depth = false) -> tgx::RenderTarget {
        auto target = tgx::RenderTarget::create(size, {.filter = tgx::TextureFilter::nearest, .depth = depth});
        REQUIRE(target.has_value());
        return std::move(*target);
    }

    // A fresh canvas drawing into the target, spanning its pixels.
    [[nodiscard]] inline auto canvas_on(const tgx::RenderTarget &target) -> tgx::Canvas {
        tgx::Canvas canvas = tgx::Canvas::create();
        canvas.set_target(&target);
        return canvas;
    }

    // One triangle over all of clip space, green: a Device::draw that covers
    // what it is drawn into, or the viewport it is given.
    struct Cover {
        struct Vertex {
            tgx::Vec2 position;
        };

        tgx::gl::Shader shader;
        tgx::gl::Buffer<Vertex> buffer;
        tgx::gl::VertexArray vertices;

        [[nodiscard]] static auto create() -> Cover {
            auto shader = tgx::gl::Shader::from_source(
                TGX_GLSL_VERSION R"(
                    layout(location = 0) in vec2 in_position;
                    void main() { gl_Position = vec4(in_position, 0.0, 1.0); }
                )",
                TGX_GLSL_VERSION R"(
                    out vec4 out_color;
                    void main() { out_color = vec4(0.0, 1.0, 0.0, 1.0); }
                )"
            );
            REQUIRE(shader.has_value());
            constexpr std::array<Vertex, 3> corners{Vertex{{-1, -1}}, Vertex{{3, -1}}, Vertex{{-1, 3}}};
            auto buffer = tgx::gl::Buffer<Vertex>::create(corners);
            REQUIRE(buffer.has_value());
            auto vertices = tgx::gl::VertexArray::create<Vertex>(
                std::array{tgx::gl::VertexAttribute::of(0, &Vertex::position)}
            );
            vertices.set_vertex_buffer(*buffer);
            return {std::move(*shader), std::move(*buffer), std::move(vertices)};
        }

        auto draw(const tgx::gl::DrawParams &params = {}) const noexcept -> void {
            app().device().draw(shader, vertices, params);
        }
    };
}
