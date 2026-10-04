#pragma once

#include "tgx/error.h"
#include "tgx/handle.h"

#include <cstddef>
#include <ranges>
#include <span>
#include <type_traits>

namespace tgx::gl {
    namespace detail {
        auto delete_buffer(GlId id) noexcept -> void;
    }

    // Anything laid out in one piece of memory whose elements can be copied
    // byte for byte: arrays, vectors, spans, strings. Other ranges have to be
    // materialised first, e.g. with std::ranges::to<std::vector>().
    template<typename R>
    concept BufferData = std::ranges::contiguous_range<R>
                         && std::ranges::sized_range<R>
                         && std::is_trivially_copyable_v<std::ranges::range_value_t<R>>;

    enum class BufferAccess {
        // Contents are fixed at creation.
        immutable,
        // Contents can be rewritten with Buffer::update.
        dynamic,
    };

    // A GPU buffer whose size is fixed at creation and whose contents may
    // change only when created as dynamic.
    //
    // Lives inside the Device: created after it, destroyed before it.
    class Buffer {
    public:
        // Uninitialised storage of the given size; only useful as dynamic.
        [[nodiscard]] static auto create(
            std::size_t size,
            BufferAccess access
        ) noexcept -> Result<Buffer>;

        // Storage sized and filled from the data. The data is copied at once,
        // so a temporary is fine.
        template<BufferData R>
        [[nodiscard]] static auto create(
            R &&data,
            BufferAccess access = BufferAccess::immutable
        ) noexcept -> Result<Buffer> {
            return create_bytes(std::as_bytes(std::span{data}), access);
        }

        Buffer(const Buffer &) = delete;
        auto operator=(const Buffer &) -> Buffer & = delete;

        Buffer(Buffer &&) noexcept = default;
        auto operator=(Buffer &&) noexcept -> Buffer & = default;

        // Overwrites the bytes starting at byte_offset; only for dynamic buffers.
        template<BufferData R>
        auto update(std::size_t byte_offset, R &&data) noexcept -> void {
            update_bytes(byte_offset, std::as_bytes(std::span{data}));
        }

        [[nodiscard]] auto id() const noexcept -> GlId { return m_handle.get(); }

        [[nodiscard]] auto size() const noexcept -> std::size_t { return m_size; }

        [[nodiscard]] auto access() const noexcept -> BufferAccess { return m_access; }

    private:
        [[nodiscard]] static auto create_bytes(
            std::span<const std::byte> data,
            BufferAccess access
        ) noexcept -> Result<Buffer>;

        auto update_bytes(std::size_t byte_offset, std::span<const std::byte> data) noexcept -> void;

        Buffer(GlId id, std::size_t size, BufferAccess access) noexcept
            : m_handle{id}, m_size{size}, m_access{access} {
        }

        tgx::detail::Handle<detail::delete_buffer> m_handle;
        std::size_t m_size{0};
        BufferAccess m_access{BufferAccess::immutable};
    };
}
