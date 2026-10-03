#pragma once

#include <cstdint>
#include <utility>

namespace tgx {
    // The name GL gives an object: what Texture::id() and the gl:: resources
    // hand out, the way out to raw GL. 0 is no object.
    using GlId = std::uint32_t;
}

namespace tgx::detail {
    // Owns one GL object: deletes it on destruction, moves, never copies. What
    // the resources are made of, not part of the API.
    template<void (*Delete)(GlId) noexcept>
    class Handle {
    public:
        Handle() noexcept = default;

        explicit Handle(GlId id) noexcept : m_id{id} {
        }

        Handle(const Handle &) = delete;
        auto operator=(const Handle &) -> Handle & = delete;

        Handle(Handle &&other) noexcept : m_id{std::exchange(other.m_id, 0)} {
        }

        auto operator=(Handle &&other) noexcept -> Handle & {
            if (this == &other) {
                return *this;
            }

            reset();
            m_id = std::exchange(other.m_id, 0);

            return *this;
        }

        ~Handle() {
            reset();
        }

        [[nodiscard]] auto get() const noexcept -> GlId { return m_id; }

        [[nodiscard]] explicit operator bool() const noexcept { return m_id != 0; }

    private:
        auto reset() noexcept -> void {
            if (m_id != 0) {
                Delete(m_id);
                m_id = 0;
            }
        }

        GlId m_id{0};
    };
}
