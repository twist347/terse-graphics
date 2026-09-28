#pragma once

#include "tgx/gl/handle.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tgx {
    class Device;
}

namespace tgx::gl {
    class Buffer;

    namespace detail {
        auto delete_vertex_array(GlId id) noexcept -> void;
    }

    // How one attribute is stored in the buffer and read by the shader. Names
    // follow WebGPU: component type, then count.
    enum class VertexFormat : std::int32_t {
        float32,
        float32x2,
        float32x3,
        float32x4,
        // Four bytes read as floats in [0, 1]; a Color fits as is.
        unorm8x4,
        uint32,
        sint32
    };

    struct VertexAttribute {
        // Matches layout(location = N) in the vertex shader.
        std::uint32_t location{0};
        VertexFormat format{VertexFormat::float32};
        // Byte offset inside one vertex, usually offsetof(Vertex, field).
        std::uint32_t offset{0};
    };

    enum class IndexType : std::int32_t {
        uint16,
        uint32
    };

    // Describes interleaved vertices of one fixed layout and which buffers feed
    // them. The layout is set at creation; buffers can be swapped later.
    //
    // Buffers are borrowed, not owned: they must outlive every draw that uses
    // this vertex array. The Device in create() is proof that GL functions are
    // loaded; it is not stored.
    class VertexArray {
    public:
        [[nodiscard]] static auto create(
            Device &device,
            std::uint32_t stride,
            std::span<const VertexAttribute> attributes
        ) noexcept -> VertexArray;

        auto set_vertex_buffer(const Buffer &buffer, std::size_t byte_offset = 0) noexcept -> void;
        auto set_index_buffer(const Buffer &buffer, IndexType type) noexcept -> void;

        [[nodiscard]] auto id() const noexcept -> GlId;
        [[nodiscard]] auto stride() const noexcept -> std::uint32_t;
        [[nodiscard]] auto has_index_buffer() const noexcept -> bool;
        [[nodiscard]] auto index_type() const noexcept -> IndexType;

    private:
        VertexArray(GlId id, std::uint32_t stride) noexcept : m_handle{id}, m_stride{stride} {
        }

        Handle<detail::delete_vertex_array> m_handle;
        std::uint32_t m_stride{0};
        IndexType m_index_type{IndexType::uint16};
        bool m_has_index_buffer{false};
    };
}
