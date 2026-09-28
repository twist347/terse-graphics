#pragma once

#include "tgx/error.h"
#include "tgx/gl/handle.h"

#include <cstddef>
#include <span>
#include <type_traits>

namespace tgx {
    class Device;
}

namespace tgx::gl {
    namespace detail {
        void delete_buffer(GlId id) noexcept;
    }

    enum class BufferAccess {
        // Contents are fixed at creation.
        immutable,
        // Contents can be rewritten with Buffer::update.
        dynamic,
    };

    // A GPU buffer with immutable storage: its size is fixed at creation, its
    // contents may change only when created as dynamic.
    //
    // The Device in create() is proof that GL functions are loaded; it is not
    // stored.
    class Buffer {
    public:
        // Uninitialised storage of the given size; only useful as dynamic.
        [[nodiscard]] static auto create(Device &device, std::size_t size, BufferAccess access) noexcept -> Result<Buffer>;

        // Storage sized and filled from the data.
        [[nodiscard]] static auto create(
            Device &device,
            std::span<const std::byte> data,
            BufferAccess access = BufferAccess::immutable
        ) noexcept -> Result<Buffer>;

        template<typename T, std::size_t Extent>
            requires std::is_trivially_copyable_v<T>
        [[nodiscard]] static auto create(
            Device &device,
            std::span<T, Extent> data,
            BufferAccess access = BufferAccess::immutable
        ) noexcept -> Result<Buffer> {
            // Explicitly dynamic: a fixed-extent byte span would pick this
            // template again and recurse forever.
            return create(device, std::span<const std::byte>{std::as_bytes(data)}, access);
        }

        void update(std::size_t byte_offset, std::span<const std::byte> data) noexcept;

        template<typename T, std::size_t Extent>
            requires std::is_trivially_copyable_v<T>
        void update(std::size_t byte_offset, std::span<T, Extent> data) noexcept {
            update(byte_offset, std::span<const std::byte>{std::as_bytes(data)});
        }

        [[nodiscard]] auto id() const noexcept -> GlId;
        [[nodiscard]] auto size() const noexcept -> std::size_t;
        [[nodiscard]] auto access() const noexcept -> BufferAccess;

    private:
        Buffer(GlId id, std::size_t size, BufferAccess access) noexcept
            : m_handle{id}, m_size{size}, m_access{access} {
        }

        Handle<detail::delete_buffer> m_handle;
        std::size_t m_size{0};
        BufferAccess m_access{BufferAccess::immutable};
    };
}
