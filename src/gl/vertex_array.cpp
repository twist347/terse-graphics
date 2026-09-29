#include "tgx/gl/vertex_array.h"

#include <cstdint>

#include <glad/gl.h>

#include "tgx/assert.h"
#include "tgx/gl/buffer.h"

namespace {
    // Lower bounds every GL 4.5 implementation guarantees; asserting against
    // them keeps a layout portable without querying the driver.
    constexpr std::uint32_t max_attributes = 16;
    constexpr std::uint32_t max_stride = 2048;
    constexpr std::uint32_t max_relative_offset = 2047;

    // The only buffer binding point in use: all attributes are interleaved.
    constexpr GLuint binding = 0;

    struct GlFormat {
        GLint components;
        GLenum type;
        GLboolean normalized;
        bool integer;
        std::uint32_t size;
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
        std::uint32_t stride,
        std::span<const VertexAttribute> attributes
    ) noexcept -> VertexArray {
        TGX_ASSERT(stride > 0 && stride <= max_stride);
        TGX_ASSERT(!attributes.empty());

        GLuint id = 0;
        glCreateVertexArrays(1, &id);

        for (std::size_t i = 0; i < attributes.size(); ++i) {
            const VertexAttribute &attribute = attributes[i];
            const GlFormat format = to_gl(attribute.format);

            TGX_ASSERT_MSG(
                attribute.location < max_attributes,
                "attribute location {} is past the portable limit of {}",
                attribute.location,
                max_attributes
            );
            TGX_ASSERT_MSG(
                attribute.offset <= max_relative_offset
                && attribute.offset <= stride
                && format.size <= stride - attribute.offset,
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

            glEnableVertexArrayAttrib(id, attribute.location);
            if (format.integer) {
                glVertexArrayAttribIFormat(id, attribute.location, format.components, format.type, attribute.offset);
            } else {
                glVertexArrayAttribFormat(
                    id,
                    attribute.location,
                    format.components,
                    format.type,
                    format.normalized,
                    attribute.offset
                );
            }
            glVertexArrayAttribBinding(id, attribute.location, binding);
        }

        return VertexArray{id, stride};
    }

    auto VertexArray::set_vertex_buffer(const Buffer &buffer, std::size_t byte_offset) noexcept -> void {
        TGX_ASSERT(m_handle);
        TGX_ASSERT(byte_offset <= buffer.size());

        m_vertex_count = (buffer.size() - byte_offset) / m_stride;
        glVertexArrayVertexBuffer(
            m_handle.get(),
            binding,
            buffer.id(),
            static_cast<GLintptr>(byte_offset),
            static_cast<GLsizei>(m_stride)
        );
    }

    auto VertexArray::set_index_buffer(const Buffer &buffer, IndexType type) noexcept -> void {
        TGX_ASSERT(m_handle);

        glVertexArrayElementBuffer(m_handle.get(), buffer.id());
        m_index_type = type;
        m_has_index_buffer = true;
        // Rounded down: trailing bytes that do not make a whole index are unused.
        m_index_count = buffer.size() / (type == IndexType::uint32 ? 4 : 2);
    }

    auto VertexArray::id() const noexcept -> GlId {
        TGX_ASSERT(m_handle);

        return m_handle.get();
    }

    auto VertexArray::stride() const noexcept -> std::uint32_t {
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
