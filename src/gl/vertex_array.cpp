#include "tgx/gl/vertex_array.h"

#include "tgx/assert.h"

#include "tgx/gl/shader.h"

#include "context.h"
#include "vertex_array_internal.h"

#include <glad/gl.h>

#include <algorithm>

namespace {
    // GL 3.3 has no stride limit to query; this is the lower bound 4.4+ drivers
    // guarantee for GL_MAX_VERTEX_ATTRIB_STRIDE, so a layout within it stays
    // portable to them too.
    constexpr std::size_t max_stride = 2048;

    using tgx::gl::detail::ComponentKind;

    // How GL reads a format, and what a shader input of it must be: the one
    // table of formats.
    struct GlFormat {
        GLint components;
        GLenum type;
        GLboolean normalized;
        ComponentKind kind;
        std::size_t size;
    };

    [[nodiscard]] constexpr auto to_gl(tgx::gl::VertexFormat format) noexcept -> GlFormat {
        using enum tgx::gl::VertexFormat;
        using enum ComponentKind;
        switch (format) {
            case float32: return {1, GL_FLOAT, GL_FALSE, floating, 4};
            case float32x2: return {2, GL_FLOAT, GL_FALSE, floating, 8};
            case float32x3: return {3, GL_FLOAT, GL_FALSE, floating, 12};
            case float32x4: return {4, GL_FLOAT, GL_FALSE, floating, 16};
            case unorm8x4: return {4, GL_UNSIGNED_BYTE, GL_TRUE, floating, 4};
            case uint32: return {1, GL_UNSIGNED_INT, GL_FALSE, uint, 4};
            case sint32: return {1, GL_INT, GL_FALSE, sint, 4};
        }
        return {1, GL_FLOAT, GL_FALSE, floating, 4};
    }

    [[maybe_unused, nodiscard]] constexpr auto kind_name(ComponentKind kind) noexcept -> const char * {
        switch (kind) {
            case ComponentKind::floating: return "float";
            case ComponentKind::sint: return "int";
            case ComponentKind::uint: return "uint";
        }
        return "unknown";
    }
}

namespace tgx::gl {
    auto detail::delete_vertex_array(GlId id) noexcept -> void {
        tgx::detail::context().forget_vertex_array(id);
        glDeleteVertexArrays(1, &id);
    }

    auto VertexArray::create(
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
                attribute.location, VertexArray::max_attributes
            );
            TGX_ASSERT_MSG(
                attribute.offset <= stride && format.size <= stride - attribute.offset,
                "attribute at location {} (offset {}, {} bytes) does not fit a {}-byte vertex",
                attribute.location, attribute.offset, format.size, stride
            );
            TGX_ASSERT_MSG(
                std::ranges::find(
                    attributes.first(i), attribute.location, &VertexAttribute::location
                )
                == attributes.first(i).end(),
                "attribute location {} is used twice",
                attribute.location
            );
        }

        GLuint id = 0;
        glGenVertexArrays(1, &id);

        // Enabling is VAO state, so it needs the VAO bound. It stays bound:
        // tgx binds GL_ELEMENT_ARRAY_BUFFER, the one buffer binding that is
        // VAO state, only to attach an index buffer here, and edits buffers
        // through GL_COPY_WRITE_BUFFER, which no VAO keeps.
        tgx::detail::context().bind_vertex_array(id);
        for (const VertexAttribute &attribute: attributes) {
            glEnableVertexAttribArray(attribute.location);
        }

        return VertexArray{id, stride, attributes};
    }

    VertexArray::VertexArray(GlId id, std::size_t stride, std::span<const VertexAttribute> attributes) noexcept
        : m_handle{id},
          m_stride{stride},
          m_attribute_count{attributes.size()} {
        std::ranges::copy(attributes, m_attributes.begin());
    }

    auto VertexArray::attach_vertex_buffer(
        GlId buffer,
        std::size_t byte_offset,
        std::size_t vertex_count
    ) noexcept -> void {
        m_vertex_count = vertex_count;

        // The ARRAY_BUFFER binding is not VAO state: each pointer call below
        // captures it, together with the offset and stride.
        tgx::detail::context().bind_vertex_array(m_handle.get());
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        for (std::size_t i = 0; i < m_attribute_count; ++i) {
            const VertexAttribute &attribute = m_attributes[i];
            const GlFormat format = to_gl(attribute.format);

            // GL takes the offset into the buffer where a pointer used to go.
            const auto *offset = reinterpret_cast<const void *>(byte_offset + attribute.offset);
            // Fits: the stride is asserted against max_stride at creation.
            const auto stride = static_cast<GLsizei>(m_stride);
            if (format.kind != ComponentKind::floating) {
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
    }

    auto VertexArray::attach_index_buffer(GlId buffer, IndexType type, std::size_t index_count) noexcept -> void {
        // Unlike ARRAY_BUFFER, this binding is VAO state: binding it with the
        // VAO bound is what attaches it.
        tgx::detail::context().bind_vertex_array(m_handle.get());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);

        m_index_type = type;
        m_has_index_buffer = true;
        m_index_count = index_count;
    }

    auto detail::check_vertex_inputs(const Shader &shader, const VertexArray &vertices) noexcept -> void {
        const auto attributes = vertices.attributes();
        for (const auto &input: vertex_inputs(shader)) {
            for (std::uint32_t slot = 0; slot < input.slots; ++slot) {
                const std::uint32_t location = input.location + slot;
                const auto attribute = std::ranges::find(attributes, location, &VertexAttribute::location);
                TGX_ASSERT_MSG(
                    attribute != attributes.end(),
                    "vertex input '{}' reads location {}, which the vertex array has no attribute for",
                    input.name, location
                );
                TGX_ASSERT_MSG(
                    to_gl(attribute->format).kind == input.kind,
                    "vertex input '{}' at location {} is {}, but its attribute is {}",
                    input.name, location, kind_name(input.kind), kind_name(to_gl(attribute->format).kind)
                );
            }
        }
    }
}
