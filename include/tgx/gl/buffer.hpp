#pragma once

#include "tgx/core/assert.hpp"
#include "tgx/core/error.hpp"
#include "tgx/core/handle.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace tgx::gl {
    enum class BufferAccess : std::int32_t {
        // Contents are fixed at creation.
        immutable,
        // Contents can be rewritten with Buffer::update.
        dynamic,
    };

    // What a buffer can hold: values copied to the GPU byte for byte.
    template<typename T>
    concept BufferElement = std::is_trivially_copyable_v<T> && std::is_same_v<T, std::remove_cv_t<T>>;

    namespace detail {
        auto delete_buffer(GlId id) noexcept -> void;

        // The untyped core of Buffer<T>, in bytes. data may be null for
        // uninitialized storage.
        [[nodiscard]] auto create_buffer(
            std::size_t byte_size,
            const void *data,
            BufferAccess access
        ) noexcept -> Result<GlId>;

        auto update_buffer(GlId id, std::size_t byte_offset, std::span<const std::byte> data) noexcept -> void;
    }

    // A GPU buffer of Ts, whose size is fixed at creation and whose contents
    // may change only when created as dynamic. Sizes and offsets count Ts, not
    // bytes. T is what the buffer feeds: the vertex struct for vertices,
    // std::uint16_t or std::uint32_t for indices, one buffer for each.
    //
    // Creating one fails with Error::out_of_memory when the GPU has no room for
    // it, Error::platform when the driver fails otherwise.
    //
    // Lives inside the Device: created after it, destroyed before it.
    template<BufferElement T>
    class Buffer {
    public:
        // Uninitialized storage for count Ts, to be filled with update(): always
        // dynamic, as an immutable one could never be filled.
        [[nodiscard]] static auto create(std::size_t count) noexcept -> Result<Buffer> {
            return make(count, nullptr, BufferAccess::dynamic);
        }

        // Storage sized and filled from the data: an array, a vector, a span.
        // The data is copied at once, so a temporary is fine.
        [[nodiscard]] static auto create(
            std::span<const T> data,
            BufferAccess access = BufferAccess::immutable
        ) noexcept -> Result<Buffer> {
            return make(data.size(), data.data(), access);
        }

        Buffer(const Buffer &) = delete;
        auto operator=(const Buffer &) -> Buffer & = delete;

        Buffer(Buffer &&) noexcept = default;
        auto operator=(Buffer &&) noexcept -> Buffer & = default;

        // Overwrites the Ts starting at first; only for dynamic buffers.
        auto update(std::size_t first, std::span<const T> data) noexcept -> void {
            TGX_ASSERT_MSG(m_access == BufferAccess::dynamic, "updating an immutable buffer");
            TGX_ASSERT_MSG(
                first <= m_size && data.size() <= m_size - first,
                "update of {} elements at {} overruns a buffer of {}",
                data.size(), first, m_size
            );

            detail::update_buffer(m_handle.get(), first * sizeof(T), std::as_bytes(data));
        }

        [[nodiscard]] auto id() const noexcept -> GlId { return m_handle.get(); }

        // How many Ts it holds.
        [[nodiscard]] auto size() const noexcept -> std::size_t { return m_size; }

        [[nodiscard]] auto access() const noexcept -> BufferAccess { return m_access; }

    private:
        [[nodiscard]] static auto make(
            std::size_t count,
            const void *data,
            BufferAccess access
        ) noexcept -> Result<Buffer> {
            TGX_ASSERT_MSG(
                count <= std::numeric_limits<std::size_t>::max() / sizeof(T),
                "{} elements of {} bytes overflow a size",
                count, sizeof(T)
            );

            return detail::create_buffer(count * sizeof(T), data, access).transform([&](GlId id) {
                return Buffer{id, count, access};
            });
        }

        Buffer(GlId id, std::size_t size, BufferAccess access) noexcept
            : m_handle{id}, m_size{size}, m_access{access} {
        }

        tgx::detail::Handle<detail::delete_buffer> m_handle;
        std::size_t m_size{0};
        BufferAccess m_access{BufferAccess::immutable};
    };
}
