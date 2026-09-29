#include "tgx/gl/vertex_array.h"

#include <algorithm>
#include <cstdint>

#include <glad/gl.h>

#include "tgx/assert.h"
#include "tgx/gl/buffer.h"

namespace {
    using tgx::gl::VertexArray;

    // GL 3.3 has no stride limit to query; this is the lower bound 4.4+ drivers
    // guarantee for GL_MAX_VERTEX_ATTRIB_STRIDE, so a layout within it stays
    // portable to them too.
    constexpr std::size_t max_stride = 2048;

    struct GlFormat {
        GLint components;
        GLenum type;
        GLboolean normalized;
        bool integer;
        std::size_t size;
    };

    [[nodiscard]] constexpr auto to_gl(tgx::gl::VertexFormat format) noexcept -> GlFormat {
        using enum tgx::gl::VertexFormat;
        switch (format) {
            case float32: return {1, GL_FLOAT, GL_FALSE, false, 4};
            case float32x2: return {2, GL_FLOAT, GL_FALSE, false, 8};
            case float32x3: return {3, GL_FLOAT, GL_FALSE, false, 12};
            case float32x4: return {4, GL_FLOAT, GL_FALSE, false, 16};
            case unorm8x4: return {4, GL_UNSIGNED_BYTE, GL_TRUE, false, 4};
            case uint32: return {1, GL_UNSIGNED_INT, GL_FALSE, true, 4};
            case sint32: return {1, GL_INT, GL_FALSE, true, 4};
        }
        return {1, GL_FLOAT, GL_FALSE, false, 4};
    }
}

namespace tgx::gl {
    auto detail::delete_vertex_array(GlId id) noexcept -> void {
        glDeleteVertexArrays(1, &id);
    }

    auto VertexArray::create(
        Device &,
        std::size_t stride,
        std::span<const VertexAttribute> attributes
    ) noexcept -> VertexArray {
        TGX_ASSERT(stride > 0 && stride <= max_stride);
        TGX_ASSERT(!attributes.empty() && attributes.size() <= VertexArray::max_attributes);

        for (std::size_t i = 0; i < attributes.size(); ++i) {
            const VertexAttribute &attribute = attributes[i];
            const GlFormat format = to_gl(attribute.format);

            TGX_ASSERT_MSG(
                attribute.location < VertexArray::max_attributes,
                "attribute location {} is past the portable limit of {}",
                attribute.location,
                VertexArray::max_attributes
            );
            TGX_ASSERT_MSG(
                attribute.offset <= stride && format.size <= stride - attribute.offset,
                "attribute at location {} (offset {}, {} bytes) does not fit a {}-byte vertex",
                attribute.location,
                attribute.offset,
                format.size,
                stride
            );
            for (std::size_t j = 0; j < i; ++j) {
                TGX_ASSERT_MSG(
                    attributes[j].location != attribute.location,
                    "attribute location {} is used twice",
                    attribute.location
                );
            }
        }

        GLuint id = 0;
        glGenVertexArrays(1, &id);

        // Enabling is VAO state, so it needs the VAO bound. VAO 0 is restored
        // after every edit here, so no later bind of an index buffer lands in
        // this one by accident.
        glBindVertexArray(id);
        for (const VertexAttribute &attribute : attributes) {
            glEnableVertexAttribArray(attribute.location);
        }
        glBindVertexArray(0);

        return VertexArray{id, stride, attributes};
    }

    VertexArray::VertexArray(GlId id, std::size_t stride, std::span<const VertexAttribute> attributes) noexcept
        : m_handle{id},
          m_stride{stride},
          m_attribute_count{attributes.size()} {
        std::ranges::copy(attributes, m_attributes.begin());
    }

    auto VertexArray::set_vertex_buffer(const Buffer &buffer, std::size_t byte_offset) noexcept -> void {
        TGX_ASSERT(m_handle);
        TGX_ASSERT(byte_offset <= buffer.size());

        m_vertex_count = (buffer.size() - byte_offset) / m_stride;

        // The ARRAY_BUFFER binding is not VAO state: each pointer call below
        // captures it, together with the offset and stride.
        glBindVertexArray(m_handle.get());
        glBindBuffer(GL_ARRAY_BUFFER, buffer.id());
        for (std::size_t i = 0; i < m_attribute_count; ++i) {
            const VertexAttribute &attribute = m_attributes[i];
            const GlFormat format = to_gl(attribute.format);

            // GL takes the offset into the buffer where a pointer used to go.
            const auto *offset = reinterpret_cast<const void *>(byte_offset + attribute.offset);
            // Fits: the stride is asserted against max_stride at creation.
            const auto stride = static_cast<GLsizei>(m_stride);
            if (format.integer) {
                glVertexAttribIPointer(attribute.location, format.components, format.type, stride, offset);
            } else {
                glVertexAttribPointer(
                    attribute.location,
                    format.components,
                    format.type,
                    format.normalized,
                    stride,
                    offset
                );
            }
        }
        glBindVertexArray(0);
    }

    auto VertexArray::set_index_buffer(const Buffer &buffer, IndexType type) noexcept -> void {
        TGX_ASSERT(m_handle);

        // Unlike ARRAY_BUFFER, this binding is VAO state: binding it with the
        // VAO bound is what attaches it.
        glBindVertexArray(m_handle.get());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.id());
        glBindVertexArray(0);

        m_index_type = type;
        m_has_index_buffer = true;
        // Rounded down: trailing bytes that do not make a whole index are unused.
        m_index_count = buffer.size() / (type == IndexType::uint32 ? 4 : 2);
    }

    auto VertexArray::id() const noexcept -> GlId {
        TGX_ASSERT(m_handle);

        return m_handle.get();
    }

    auto VertexArray::stride() const noexcept -> std::size_t {
        TGX_ASSERT(m_handle);

        return m_stride;
    }

    auto VertexArray::vertex_count() const noexcept -> std::size_t {
        TGX_ASSERT(m_handle);

        return m_vertex_count;
    }

    auto VertexArray::index_count() const noexcept -> std::size_t {
        TGX_ASSERT(m_handle);
        TGX_ASSERT_MSG(m_has_index_buffer, "no index buffer is attached");

        return m_index_count;
    }

    auto VertexArray::has_index_buffer() const noexcept -> bool {
        TGX_ASSERT(m_handle);

        return m_has_index_buffer;
    }

    auto VertexArray::index_type() const noexcept -> IndexType {
        TGX_ASSERT(m_handle);
        TGX_ASSERT_MSG(m_has_index_buffer, "no index buffer is attached");

        return m_index_type;
    }
}
