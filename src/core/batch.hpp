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

    // One vertex of 2D drawing, in the layout the built-in shader reads and a
    // custom one must too (Canvas::set_shader).
    struct BatchVertex {
        Vec2 position;
        Vec2 uv;
        Color color;
    };

    // What the vertices in the batch are drawn with. Vertices are only added to
    // a batch of the same state; another state draws what is there first.
    //
    // By ids rather than by objects: a Texture or a Shader moved elsewhere
    // keeps its id, so the vertices it was added with stay good.
    struct BatchState {
        // What the canvas draws into.
        Target target{};
        // 0 for the built-in texture: the default font, and white for shapes,
        // whose color comes from the vertices alone.
        GlId texture{0};
        Blend blend{Blend::alpha};
        // 0 for the built-in shader.
        GlId program{0};
        // The locations of a custom shader's u_projection and u_texture (-1
        // when it has none); the built-in one keeps its own.
        std::int32_t u_projection{-1};
        std::int32_t u_texture{-1};
        // From the vertices' coordinates to clip space: u_projection.
        Mat4 transform{};
        // The part of the target the canvas covers, in pixels from the
        // top-left.
        gl::Viewport viewport{};

        [[nodiscard]] auto operator==(const BatchState &) const noexcept -> bool = default;
    };

    // Vertices waiting to be drawn together, owned by the Context: it draws them
    // before anything else of its own reaches the framebuffer, so the picture
    // follows the order of the calls. The Canvas fills it.
    class Batch {
    public:
        [[nodiscard]] static auto create() -> Result<Batch>;

        Batch(const Batch &) = delete;
        auto operator=(const Batch &) -> Batch & = delete;

        Batch(Batch &&) noexcept = default;
        auto operator=(Batch &&) noexcept -> Batch & = default;

        // Room for this many more vertices and indices drawn with the state,
        // drawing what is there first if the state differs or they would not
        // fit. Returns the index the first of the new vertices will have; push
        // exactly that many after.
        [[nodiscard]] auto reserve(
            Context &context,
            const BatchState &state,
            std::size_t vertex_count,
            std::size_t index_count
        ) noexcept -> std::uint16_t;

        auto push_vertex(const BatchVertex &vertex) noexcept -> void { m_vertices.push_back(vertex); }

        auto push_index(std::uint16_t index) noexcept -> void { m_indices.push_back(index); }

        // Draws what is collected, if anything.
        auto flush(Context &context) noexcept -> void;

        // Whether what is collected is drawn with the texture, or into it as a
        // render target's, or with the program of a custom shader.
        [[nodiscard]] auto uses_texture(GlId texture) const noexcept -> bool;

        [[nodiscard]] auto uses_shader(GlId program) const noexcept -> bool;

        // Whether what is collected is drawn into the render target.
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

        gl::Shader m_shader;
        // The location of its u_projection.
        std::int32_t m_u_projection;
        gl::Buffer<BatchVertex> m_vertex_buffer;
        gl::Buffer<std::uint16_t> m_index_buffer;
        gl::VertexArray m_vertex_array;
        // The default font's glyphs and a white block. Shapes sample the white,
        // so they share the shader with textured drawing and their color comes
        // from the vertices alone; text and shapes in a row are one draw.
        Texture m_builtin;

        std::vector<BatchVertex> m_vertices;
        std::vector<std::uint16_t> m_indices;
        BatchState m_state{};
    };

    // Per draw. Indices are 16-bit, so vertices stay below 65536. A circle
    // takes about 3 indices per vertex, the most of any shape.
    inline constexpr std::size_t batch_max_vertices = 16384;
    inline constexpr std::size_t batch_max_indices = batch_max_vertices * 3;
}
