#pragma once

#include "tgx/core/assert.hpp"
#include "tgx/core/color.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/math.hpp"

#include "tgx/gl/buffer.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace tgx::gl {
    namespace detail {
        auto delete_vertex_array(GlId id) noexcept -> void;
    }

    // How one attribute is stored and read. Names follow WebGPU: component
    // type, then count.
    enum class VertexFormat : std::int32_t {
        float32,
        float32x2,
        float32x3,
        float32x4,
        // Four bytes read as floats in [0, 1]. A Color fits as is.
        unorm8x4,
        uint32,
        sint32,
    };

    // Copied byte for byte, with a plain layout, so field offsets match what
    // the GPU reads.
    template<typename T>
    concept VertexElement = BufferElement<T> && std::is_standard_layout_v<T>;

    // Fails to compile for a type with no natural format; describe such a
    // field by hand.
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
        // Bytes, usually offsetof(Vertex, field).
        std::size_t offset{0};

        // Takes the format and offset from the field itself:
        //
        //     VertexAttribute::of(0, &Vertex::position)
        //
        // Not constexpr: a member pointer's offset is measured on a real
        // object.
        template<VertexElement Vertex, typename Field>
            requires std::is_default_constructible_v<Vertex>
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
        uint32,
    };

    template<typename T>
    concept IndexElement = std::same_as<T, std::uint16_t> || std::same_as<T, std::uint32_t>;

    template<IndexElement I>
    [[nodiscard]] consteval auto index_type_of() noexcept -> IndexType {
        return std::same_as<I, std::uint32_t> ? IndexType::uint32 : IndexType::uint16;
    }

    // Interleaved vertices with a layout fixed at creation, and the buffers
    // they come from. Buffers can be swapped.
    //
    // Buffers are borrowed: they must outlive every draw that uses them.
    // Create it after the Device and destroy it before the Device.
    class VertexArray {
    public:
        // Attribute locations must be below this: the GL_MAX_VERTEX_ATTRIBS
        // that GL 3.3 guarantees.
        static constexpr std::uint32_t max_attributes = 16;

        [[nodiscard]] static auto create(
            std::size_t stride,
            std::span<const VertexAttribute> attributes
        ) noexcept -> VertexArray;

        // The stride is sizeof(Vertex).
        template<VertexElement Vertex>
        [[nodiscard]] static auto create(
            std::span<const VertexAttribute> attributes
        ) noexcept -> VertexArray {
            return create(sizeof(Vertex), attributes);
        }

        VertexArray(const VertexArray &) = delete;
        auto operator=(const VertexArray &) -> VertexArray & = delete;

        VertexArray(VertexArray &&) noexcept = default;
        auto operator=(VertexArray &&) noexcept -> VertexArray & = default;

        // Vertices start at index first. Asserts that sizeof(V) is the
        // stride.
        template<VertexElement V>
        auto set_vertex_buffer(const Buffer<V> &buffer, std::size_t first = 0) noexcept -> void {
            TGX_ASSERT_MSG(
                sizeof(V) == m_stride,
                "a buffer of {}-byte elements feeds {}-byte vertices",
                sizeof(V), m_stride
            );
            TGX_ASSERT(first <= buffer.size());

            attach_vertex_buffer(buffer.id(), first * sizeof(V), buffer.size() - first);
        }

        // From now on, draws use the indices.
        template<IndexElement I>
        auto set_index_buffer(const Buffer<I> &buffer) noexcept -> void {
            attach_index_buffer(buffer.id(), index_type_of<I>(), buffer.size());
        }

        [[nodiscard]] auto id() const noexcept -> GlId { return m_handle.get(); }

        [[nodiscard]] auto stride() const noexcept -> std::size_t { return m_stride; }

        // Counted from the first one used. 0 until a buffer is attached.
        [[nodiscard]] auto vertex_count() const noexcept -> std::size_t { return m_vertex_count; }

        // 0 until a buffer is attached.
        [[nodiscard]] auto index_count() const noexcept -> std::size_t { return m_index_count; }

        [[nodiscard]] auto has_index_buffer() const noexcept -> bool { return m_has_index_buffer; }

        [[nodiscard]] auto index_type() const noexcept -> IndexType {
            TGX_ASSERT_MSG(m_has_index_buffer, "no index buffer is attached");

            return m_index_type;
        }

        // The layout given at creation.
        [[nodiscard]] auto attributes() const noexcept -> std::span<const VertexAttribute> {
            return std::span{m_attributes}.first(m_attribute_count);
        }

    private:
        VertexArray(GlId id, std::size_t stride, std::span<const VertexAttribute> attributes) noexcept;

        auto attach_vertex_buffer(GlId buffer, std::size_t byte_offset, std::size_t vertex_count) noexcept -> void;

        auto attach_index_buffer(GlId buffer, IndexType type, std::size_t index_count) noexcept -> void;

        tgx::detail::Handle<detail::delete_vertex_array> m_handle;
        std::size_t m_stride{0};
        // GL 3.3 ties an attribute's format to its buffer, so the layout is
        // kept to give to GL again when the buffer changes.
        std::array<VertexAttribute, max_attributes> m_attributes{};
        std::size_t m_attribute_count{0};
        // Stored on attach. Buffer sizes are fixed.
        std::size_t m_vertex_count{0};
        std::size_t m_index_count{0};
        IndexType m_index_type{IndexType::uint16};
        bool m_has_index_buffer{false};
    };
}
