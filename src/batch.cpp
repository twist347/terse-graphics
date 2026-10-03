#include "batch.h"

#include "tgx/assert.h"
#include "tgx/image.h"

#include "tgx/gl/draw.h"
#include "tgx/gl/version.h"

#include "context.h"

#include <glad/gl.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec2 in_uv;
        layout(location = 2) in vec4 in_color;

        uniform mat4 u_projection;

        out vec2 uv;
        out vec4 color;

        void main() {
            gl_Position = u_projection * vec4(in_position, 0.0, 1.0);
            uv = in_uv;
            color = in_color;
        }
    )";

    constexpr const char *fragment_source = TGX_GLSL_VERSION R"(
        in vec2 uv;
        in vec4 color;

        uniform sampler2D u_texture;

        out vec4 out_color;

        void main() {
            out_color = texture(u_texture, uv) * color;
        }
    )";

    using tgx::detail::batch_max_indices;
    using tgx::detail::batch_max_vertices;

    static_assert(batch_max_vertices - 1 <= std::numeric_limits<std::uint16_t>::max());
}

namespace tgx::detail {
    auto Batch::create() -> Result<Batch> {
        auto shader = gl::Shader::from_source(vertex_source, fragment_source);
        if (!shader) {
            // Ours, and checked by the examples on every platform tgx runs on;
            // a driver refusing it is the platform's failure.
            TGX_ASSERT_MSG(false, "the 2D batch shader failed: {}", shader.error());
            return std::unexpected{Error::platform};
        }
        const std::int32_t u_projection = projection_location(*shader);
        shader->set(shader->uniform<gl::TextureSlot>("u_texture"), {0});

        auto vertex_buffer = gl::Buffer::create(
            batch_max_vertices * sizeof(BatchVertex), gl::BufferAccess::dynamic
        );
        if (!vertex_buffer) {
            return std::unexpected{vertex_buffer.error()};
        }
        auto index_buffer = gl::Buffer::create(
            batch_max_indices * sizeof(std::uint16_t), gl::BufferAccess::dynamic
        );
        if (!index_buffer) {
            return std::unexpected{index_buffer.error()};
        }

        const std::array layout{
            gl::VertexAttribute::of(0, &BatchVertex::position),
            gl::VertexAttribute::of(1, &BatchVertex::uv),
            gl::VertexAttribute::of(2, &BatchVertex::color),
        };
        auto vertex_array = gl::VertexArray::create<BatchVertex>(layout);
        vertex_array.set_vertex_buffer(*vertex_buffer);
        vertex_array.set_index_buffer(*index_buffer, gl::IndexType::uint16);

        auto white = Texture::create(Image::create({1, 1}, colors::white));
        if (!white) {
            return std::unexpected{white.error()};
        }

        return Batch{
            std::move(*shader),
            u_projection,
            std::move(*vertex_buffer),
            std::move(*index_buffer),
            std::move(vertex_array),
            std::move(*white),
        };
    }

    Batch::Batch(
        gl::Shader shader,
        std::int32_t u_projection,
        gl::Buffer vertex_buffer,
        gl::Buffer index_buffer,
        gl::VertexArray vertex_array,
        Texture white
    )
        : m_shader{std::move(shader)},
          m_u_projection{u_projection},
          m_vertex_buffer{std::move(vertex_buffer)},
          m_index_buffer{std::move(index_buffer)},
          m_vertex_array{std::move(vertex_array)},
          m_white{std::move(white)} {
        // Filled up to here and no further: adding a shape never allocates.
        m_vertices.reserve(batch_max_vertices);
        m_indices.reserve(batch_max_indices);
    }

    auto Batch::reserve(
        Context &context,
        const BatchState &state,
        std::size_t vertex_count,
        std::size_t index_count
    ) noexcept -> std::uint16_t {
        TGX_ASSERT(vertex_count <= batch_max_vertices && index_count <= batch_max_indices);

        if (state != m_state
            || m_vertices.size() + vertex_count > batch_max_vertices
            || m_indices.size() + index_count > batch_max_indices) {
            flush(context);
            m_state = state;
        }
        return static_cast<std::uint16_t>(m_vertices.size());
    }

    auto Batch::flush(Context &context) noexcept -> void {
        if (m_indices.empty()) {
            return;
        }

        m_vertex_buffer.update(0, m_vertices);
        m_index_buffer.update(0, m_indices);

        // Set on every draw: one matrix is cheap, and a custom shader may have
        // been used elsewhere in between. Straight to GL rather than through
        // Shader::set, which would come back here to flush.
        const bool custom = m_state.program != 0;
        const GlId program = custom ? m_state.program : m_shader.id();
        const std::int32_t u_projection = custom ? m_state.u_projection : m_u_projection;
        const auto floats = std::bit_cast<std::array<float, 16>>(m_state.transform);
        context.use_program(program);
        glUniformMatrix4fv(u_projection, 1, GL_FALSE, floats.data());

        context.draw({
            .program = program,
            .vertex_array = m_vertex_array.id(),
            .index_type = gl::IndexType::uint16,
            .count = m_indices.size(),
            .state = {.blend = m_state.blend},
            // The canvas covers the whole framebuffer.
            .viewport = full_viewport(),
            .textures = {m_state.texture != 0 ? m_state.texture : m_white.id()},
        });

        m_vertices.clear();
        m_indices.clear();
    }

    auto Batch::uses_texture(GlId texture) const noexcept -> bool {
        return !m_indices.empty() && m_state.texture == texture;
    }

    auto Batch::uses_shader(GlId program) const noexcept -> bool {
        return !m_indices.empty() && m_state.program == program;
    }

    auto projection_location(const gl::Shader &shader) noexcept -> std::int32_t {
        return glGetUniformLocation(shader.id(), "u_projection");
    }
}
