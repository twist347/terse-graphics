#pragma once

#include <cstdint>
#include <utility>

namespace tgx::gl {
    using GlId = std::uint32_t;

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
        void reset() noexcept {
            if (m_id != 0) {
                Delete(m_id);
                m_id = 0;
            }
        }

        GlId m_id{0};
    };
}
