#pragma once

#include "tgx/color.h"
#include "tgx/handle.h"
#include "tgx/math.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

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

    // The format a vertex field of type T is read with. Fails to compile for a
    // type with no natural format; describe such a field by hand.
    template<typename T>
    [[nodiscard]] consteval auto vertex_format_of() noexcept -> VertexFormat {
        using enum VertexFormat;
        if constexpr (std::is_same_v<T, float>) {
            return float32;
        } else if constexpr (std::is_same_v<T, Vec2>) {
            return float32x2;
        } else if constexpr (std::is_same_v<T, Vec3>) {
            return float32x3;
        } else if constexpr (std::is_same_v<T, Vec4>) {
            return float32x4;
        } else if constexpr (std::is_same_v<T, Color>) {
            return unorm8x4;
        } else if constexpr (std::is_same_v<T, std::uint32_t>) {
            return uint32;
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            return sint32;
        } else {
            static_assert(false, "no VertexFormat for this field type; fill in VertexAttribute by hand");
        }
    }

    struct VertexAttribute {
        // Matches layout(location = N) in the vertex shader.
        std::uint32_t location{0};
        VertexFormat format{VertexFormat::float32};
        // Byte offset inside one vertex, usually offsetof(Vertex, field).
        std::size_t offset{0};

        // Format and offset taken from the field itself:
        //
        //     VertexAttribute::of(0, &Vertex::position)
        //
        // Not constexpr: offsetof needs the field's name, and a member pointer
        // only gives up its offset measured on a real object.
        template<typename Vertex, typename Field>
            requires std::is_standard_layout_v<Vertex>
                     && std::is_trivially_copyable_v<Vertex>
                     && std::is_default_constructible_v<Vertex>
        [[nodiscard]] static auto of(std::uint32_t location, Field Vertex::*field) noexcept -> VertexAttribute {
            const Vertex vertex{};
            const auto *base = reinterpret_cast<const std::byte *>(&vertex);
            const auto *member = reinterpret_cast<const std::byte *>(&(vertex.*field));
            return {
                .location = location,
                .format = vertex_format_of<Field>(),
                .offset = static_cast<std::size_t>(member - base),
            };
        }
    };

    enum class IndexType : std::int32_t {
        uint16,
        uint32
    };

    // Describes interleaved vertices of one fixed layout and which buffers feed
    // them. The layout is set at creation; buffers can be swapped later.
    //
    // Buffers are borrowed, not owned: they must outlive every draw that uses
    // this vertex array. Lives inside the Device: created after it, destroyed
    // before it.
    class VertexArray {
    public:
        // Attribute locations must be below this: the lower bound every GL 3.3
        // implementation guarantees for GL_MAX_VERTEX_ATTRIBS.
        static constexpr std::uint32_t max_attributes = 16;

        [[nodiscard]] static auto create(
            std::size_t stride,
            std::span<const VertexAttribute> attributes
        ) noexcept -> VertexArray;

        // The stride is sizeof(Vertex). Vertex must be laid out plainly, so the
        // offsetof() values in the attributes mean what the GPU will read.
        template<typename Vertex>
            requires std::is_standard_layout_v<Vertex> && std::is_trivially_copyable_v<Vertex>
        [[nodiscard]] static auto create(
            std::span<const VertexAttribute> attributes
        ) noexcept -> VertexArray {
            return create(sizeof(Vertex), attributes);
        }

        VertexArray(const VertexArray &) = delete;
        auto operator=(const VertexArray &) -> VertexArray & = delete;

        VertexArray(VertexArray &&) noexcept = default;
        auto operator=(VertexArray &&) noexcept -> VertexArray & = default;

        auto set_vertex_buffer(const Buffer &buffer, std::size_t byte_offset = 0) noexcept -> void;

        auto set_index_buffer(const Buffer &buffer, IndexType type) noexcept -> void;

        [[nodiscard]] auto id() const noexcept -> GlId;

        [[nodiscard]] auto stride() const noexcept -> std::size_t;

        // Whole vertices the attached vertex buffer holds past its offset; 0
        // until one is attached.
        [[nodiscard]] auto vertex_count() const noexcept -> std::size_t;

        // Whole indices the attached index buffer holds.
        [[nodiscard]] auto index_count() const noexcept -> std::size_t;

        [[nodiscard]] auto has_index_buffer() const noexcept -> bool;

        [[nodiscard]] auto index_type() const noexcept -> IndexType;

        // The layout given at creation.
        [[nodiscard]] auto attributes() const noexcept -> std::span<const VertexAttribute>;

    private:
        VertexArray(GlId id, std::size_t stride, std::span<const VertexAttribute> attributes) noexcept;

        tgx::detail::Handle<detail::delete_vertex_array> m_handle;
        std::size_t m_stride{0};
        // GL 3.3 ties an attribute's format to the buffer it reads from, so the
        // layout is kept here and handed to GL again whenever the buffer changes.
        std::array<VertexAttribute, max_attributes> m_attributes{};
        std::size_t m_attribute_count{0};
        // Cached at attach time: buffer sizes are fixed at creation.
        std::size_t m_vertex_count{0};
        std::size_t m_index_count{0};
        IndexType m_index_type{IndexType::uint16};
        bool m_has_index_buffer{false};
    };
}
