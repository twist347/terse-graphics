#include "batch.h"

#include "tgx/assert.h"
#include "tgx/image.h"

#include "tgx/gl/device.h"
#include "tgx/gl/version.h"

#include <array>
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

    using tgx::gl::detail::batch_max_indices;
    using tgx::gl::detail::batch_max_vertices;

    static_assert(batch_max_vertices - 1 <= std::numeric_limits<std::uint16_t>::max());
}

namespace tgx::gl::detail {
    auto Batch::create() -> Result<Batch> {
        auto shader = Shader::from_source(vertex_source, fragment_source);
        if (!shader) {
            // Ours, and checked by the examples on every platform tgx runs on;
            // a driver refusing it is the platform's failure.
            TGX_ASSERT_MSG(false, "the 2D batch shader failed: {}", shader.error());
            return std::unexpected{Error::platform};
        }
        const auto u_projection = shader->uniform<Mat4>("u_projection");
        shader->set(shader->uniform<TextureSlot>("u_texture"), {0});

        auto vertex_buffer = Buffer::create(batch_max_vertices * sizeof(BatchVertex), BufferAccess::dynamic);
        if (!vertex_buffer) {
            return std::unexpected{vertex_buffer.error()};
        }
        auto index_buffer = Buffer::create(batch_max_indices * sizeof(std::uint16_t), BufferAccess::dynamic);
        if (!index_buffer) {
            return std::unexpected{index_buffer.error()};
        }

        const std::array layout{
            VertexAttribute::of(0, &BatchVertex::position),
            VertexAttribute::of(1, &BatchVertex::uv),
            VertexAttribute::of(2, &BatchVertex::color),
        };
        auto vertex_array = VertexArray::create<BatchVertex>(layout);
        vertex_array.set_vertex_buffer(*vertex_buffer);
        vertex_array.set_index_buffer(*index_buffer, IndexType::uint16);

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
        Shader shader,
        Uniform<Mat4> u_projection,
        Buffer vertex_buffer,
        Buffer index_buffer,
        VertexArray vertex_array,
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
        Device &device,
        const BatchState &state,
        std::size_t vertex_count,
        std::size_t index_count
    ) noexcept -> std::uint16_t {
        TGX_ASSERT(vertex_count <= batch_max_vertices && index_count <= batch_max_indices);

        // An empty batch takes the state afresh: the texture it points at may
        // have been replaced at the same address since.
        if (m_indices.empty()
            || state != m_state
            || m_vertices.size() + vertex_count > batch_max_vertices
            || m_indices.size() + index_count > batch_max_indices) {
            flush(device);
            m_state = state;
            m_texture_id = state.texture != nullptr ? state.texture->id() : 0;
        }
        return static_cast<std::uint16_t>(m_vertices.size());
    }

    auto Batch::flush(Device &device) noexcept -> void {
        if (m_indices.empty()) {
            return;
        }

        m_vertex_buffer.update(0, m_vertices);
        m_index_buffer.update(0, m_indices);
        const std::size_t count = m_indices.size();

        // Emptied before the draw, which itself draws the batch first: there
        // is nothing left for it then.
        m_vertices.clear();
        m_indices.clear();

        // Set on every draw: one matrix is cheap, and a custom shader may have
        // been used elsewhere in between. Its uniform is looked up here, as
        // the state holds only the shader.
        Shader &shader = m_state.shader != nullptr ? *m_state.shader : m_shader;
        const Uniform<Mat4> u_projection = m_state.shader != nullptr
            ? shader.uniform<Mat4>("u_projection")
            : m_u_projection;
        shader.set(u_projection, m_state.transform);

        device.draw(shader, m_vertex_array, {
            .count = count,
            .state = {.blend = m_state.blend},
            .textures = {m_state.texture != nullptr ? m_state.texture : &m_white},
        });
    }

    auto Batch::uses(GlId texture) const noexcept -> bool {
        return !m_indices.empty() && m_texture_id == texture;
    }
}
