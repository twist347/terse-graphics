#pragma once

#include "tgx/core/blend.hpp"
#include "tgx/core/color.hpp"
#include "tgx/core/error.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/math.hpp"
#include "tgx/core/render_target.hpp"
#include "tgx/core/texture.hpp"

#include "tgx/gl/buffer.hpp"
#include "tgx/gl/draw.hpp"
#include "tgx/gl/shader.hpp"
#include "tgx/gl/vertex_array.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tgx::detail {
    struct Context;

    // The layout the built-in shader reads, and a custom one must too
    // (Canvas::set_shader).
    struct BatchVertex {
        Vec2 position;
        Vec2 uv;
        Color color;
    };

    // What the waiting vertices are drawn with; a different state draws them
    // first. Holds ids, so a moved Texture or Shader stays valid.
    struct BatchState {
        Target target{};
        // 0: the built-in texture (the default font and white for shapes).
        GlId texture{0};
        Blend blend{Blend::alpha};
        // 0: the built-in shader.
        GlId program{0};
        // A custom shader's locations, -1 if absent. Unused by the built-in
        // shader.
        std::int32_t u_projection{-1};
        std::int32_t u_texture{-1};
        // To clip space; goes to u_projection.
        Mat4 transform{};
        // In pixels from the top-left.
        gl::Viewport viewport{};

        [[nodiscard]] auto operator==(const BatchState &) const noexcept -> bool = default;
    };

    // Vertices waiting to be drawn together. Filled by the Canvas, owned and
    // flushed by the Context before its own drawing, so call order holds.
    class Batch {
    public:
        [[nodiscard]] static auto create() -> Result<Batch>;

        Batch(const Batch &) = delete;
        auto operator=(const Batch &) -> Batch & = delete;

        Batch(Batch &&) noexcept = default;
        auto operator=(Batch &&) noexcept -> Batch & = default;

        // Draws the waiting vertices first if the state differs or the new
        // ones do not fit. Returns the first new vertex's index. Push exactly
        // that many vertices and indices afterwards.
        [[nodiscard]] auto reserve(
            Context &context,
            const BatchState &state,
            std::size_t vertex_count,
            std::size_t index_count
        ) noexcept -> std::uint16_t;

        auto push_vertex(const BatchVertex &vertex) noexcept -> void { vertices_.push_back(vertex); }

        auto push_index(std::uint16_t index) noexcept -> void { indices_.push_back(index); }

        // If any are waiting.
        auto flush(Context &context) noexcept -> void;

        // Whether waiting vertices are drawn with the texture or into it (a
        // render target's texture), or with the program.
        [[nodiscard]] auto uses_texture(GlId texture) const noexcept -> bool;

        [[nodiscard]] auto uses_shader(GlId program) const noexcept -> bool;

        // Whether waiting vertices are drawn into the framebuffer.
        [[nodiscard]] auto uses_target(GlId framebuffer) const noexcept -> bool;

    private:
        Batch(
            gl::Shader shader,
            std::int32_t u_projection,
            gl::Buffer<BatchVertex> vertex_buffer,
            gl::Buffer<std::uint16_t> index_buffer,
            gl::VertexArray vertex_array,
            Texture builtin
        );

        gl::Shader shader_;
        // The built-in shader's.
        std::int32_t u_projection_;
        gl::Buffer<BatchVertex> vertex_buffer_;
        gl::Buffer<std::uint16_t> index_buffer_;
        gl::VertexArray vertex_array_;
        // The default font and a white block that shapes sample, so text
        // and shapes in a row share one shader and one draw.
        Texture builtin_;

        std::vector<BatchVertex> vertices_;
        std::vector<std::uint16_t> indices_;
        BatchState state_{};
    };

    // Per draw. Indices are 16-bit, so under 65536 vertices. A circle uses
    // about 3 indices per vertex, the most of any shape.
    inline constexpr std::size_t batch_max_vertices = 16384;
    inline constexpr std::size_t batch_max_indices = batch_max_vertices * 3;
}
