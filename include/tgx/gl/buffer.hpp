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
        // Rewritable with Buffer::update.
        dynamic,
    };

    // Copied to the GPU byte for byte.
    template<typename T>
    concept BufferElement = std::is_trivially_copyable_v<T> && std::is_same_v<T, std::remove_cv_t<T>>;

    namespace detail {
        auto delete_buffer(GlId id) noexcept -> void;

        // Untyped, in bytes. Null data: uninitialized storage.
        [[nodiscard]] auto create_buffer(
            std::size_t byte_size,
            const void *data,
            BufferAccess access
        ) noexcept -> Result<GlId>;

        auto update_buffer(GlId id, std::size_t byte_offset, std::span<const std::byte> data) noexcept -> void;
    }

    // A GPU buffer of Ts, fixed in size. Only a dynamic one can change. Sizes
    // and offsets count Ts, not bytes. T is the vertex struct, or
    // std::uint16_t or std::uint32_t for indices, in separate buffers.
    //
    // Creation fails with Error::out_of_memory, or Error::platform for other
    // driver failures.
    //
    // Create it after the Device and destroy it before the Device.
    template<BufferElement T>
    class Buffer {
    public:
        // Uninitialized, to fill with update(). Always dynamic.
        [[nodiscard]] static auto create(std::size_t count) noexcept -> Result<Buffer> {
            return make(count, nullptr, BufferAccess::dynamic);
        }

        // Copies the data right away, so a temporary is fine.
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

        // Only for dynamic buffers.
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

        // In Ts.
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
