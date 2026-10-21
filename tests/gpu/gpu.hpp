#pragma once

// What the GPU tests share: the one App of the run, render targets to draw
// into and read back, and a draw of their own that covers what it is drawn
// into. Most tests draw into a render target of their own: its size is theirs
// to pick, whatever the window and the display's scale are.

#include "tgx/tgx.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <array>
#include <utility>

namespace tgx_test {
    // The one App of the run, made by main before any test.
    [[nodiscard]] auto app() -> tgx::App &;

    // Of the size, with nearest filtering so it shows the pixels drawn when
    // drawn itself.
    [[nodiscard]] inline auto target(tgx::Size size, bool depth = false) -> tgx::RenderTarget {
        return required(tgx::RenderTarget::create(size, {.filter = tgx::TextureFilter::nearest, .depth = depth}));
    }

    [[nodiscard]] inline auto texture(const tgx::Image &image, const tgx::TextureParams &params = {}) -> tgx::Texture {
        return required(tgx::Texture::create(image, params));
    }

    // One texel of the color.
    [[nodiscard]] inline auto plain(tgx::Color color) -> tgx::Texture {
        return texture(tgx::Image::create({1, 1}, color));
    }

    // A fresh canvas drawing into the target, spanning its pixels.
    [[nodiscard]] inline auto canvas_on(const tgx::RenderTarget &target) -> tgx::Canvas {
        auto canvas = tgx::Canvas::create();
        canvas.set_target(&target);
        return canvas;
    }

    // A shader for Cover's vertices, with a fragment source of its own.
    [[nodiscard]] inline auto cover_shader(const char *fragment_source) -> tgx::gl::Shader {
        return required(tgx::gl::Shader::from_source(
            TGX_GLSL_VERSION R"(
                layout(location = 0) in vec2 in_position;
                void main() { gl_Position = vec4(in_position, 0.0, 1.0); }
            )",
            fragment_source
        ));
    }

    // One triangle over all of clip space, green: a Device::draw that covers
    // what it is drawn into, or the viewport it is given. Counter-clockwise.
    struct Cover {
        struct Vertex {
            tgx::Vec2 position{};
        };

        tgx::gl::Shader shader;
        tgx::gl::Buffer<Vertex> buffer;
        tgx::gl::VertexArray vertices;

        [[nodiscard]] static auto create() -> Cover {
            tgx::gl::Shader shader = cover_shader(TGX_GLSL_VERSION R"(
                out vec4 out_color;
                void main() { out_color = vec4(0.0, 1.0, 0.0, 1.0); }
            )");
            constexpr std::array corners{Vertex{{-1, -1}}, Vertex{{3, -1}}, Vertex{{-1, 3}}};
            auto buffer = required(tgx::gl::Buffer<Vertex>::create(corners));
            tgx::gl::VertexArray vertices = layout();
            vertices.set_vertex_buffer(buffer);
            return {std::move(shader), std::move(buffer), std::move(vertices)};
        }

        // A vertex array for Vertex, with no buffer yet.
        [[nodiscard]] static auto layout() -> tgx::gl::VertexArray {
            return tgx::gl::VertexArray::create<Vertex>(std::array{tgx::gl::VertexAttribute::of(0, &Vertex::position)});
        }

        auto draw(const tgx::gl::DrawParams &params = {}) const noexcept -> void {
            app().device().draw(shader, vertices, params);
        }
    };
}
