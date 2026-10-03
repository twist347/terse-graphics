#pragma once

#include "tgx/blend.h"
#include "tgx/color.h"
#include "tgx/error.h"
#include "tgx/math.h"
#include "tgx/texture.h"

#include "tgx/gl/buffer.h"
#include "tgx/gl/handle.h"
#include "tgx/gl/shader.h"
#include "tgx/gl/vertex_array.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tgx::gl::detail {
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
        // 0 for the white texture: color from the vertices alone.
        GlId texture{0};
        Blend blend{Blend::alpha};
        // 0 for the built-in shader.
        GlId program{0};
        // The location of a custom shader's u_projection; the built-in one
        // keeps its own.
        std::int32_t u_projection{-1};
        // From the vertices' coordinates to clip space: u_projection.
        Mat4 transform{};

        [[nodiscard]] auto operator==(const BatchState &) const noexcept -> bool = default;
    };

    // Vertices waiting to be drawn together, owned by the Context: it draws them
    // before anything else of its own reaches the framebuffer, so the picture
    // follows the order of the calls. The Canvas fills it.
    class Batch {
    public:
        [[nodiscard]] static auto create() -> Result<Batch>;

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

        // Whether what is collected is drawn with the texture, or with the
        // program of a custom shader.
        [[nodiscard]] auto uses_texture(GlId texture) const noexcept -> bool;
        [[nodiscard]] auto uses_shader(GlId program) const noexcept -> bool;

    private:
        Batch(
            Shader shader,
            std::int32_t u_projection,
            Buffer vertex_buffer,
            Buffer index_buffer,
            VertexArray vertex_array,
            Texture white
        );

        Shader m_shader;
        // The location of its u_projection.
        std::int32_t m_u_projection;
        Buffer m_vertex_buffer;
        Buffer m_index_buffer;
        VertexArray m_vertex_array;
        // Shapes sample its one white texel, so they share the shader with
        // textured drawing and their color comes from the vertices alone.
        Texture m_white;

        std::vector<BatchVertex> m_vertices;
        std::vector<std::uint16_t> m_indices;
        BatchState m_state{};
    };

    // The location of the shader's u_projection, for BatchState: looked up
    // once, when the Canvas is given the shader. -1 if it has none.
    [[nodiscard]] auto projection_location(const Shader &shader) noexcept -> std::int32_t;

    // Per draw. Indices are 16-bit, so vertices stay below 65536. A circle
    // takes about 3 indices per vertex, the most of any shape.
    inline constexpr std::size_t batch_max_vertices = 16384;
    inline constexpr std::size_t batch_max_indices = batch_max_vertices * 3;
}
