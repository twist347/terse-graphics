#pragma once

#include <cstdint>
#include <utility>

namespace tgx {
    // A GL object's id, for raw GL calls. 0 means none.
    using GlId = std::uint32_t;
}

namespace tgx::detail {
    // Owns one GL object: move-only, deletes it on destruction. Not API.
    template<void (*Delete)(GlId) noexcept>
    class Handle {
    public:
        Handle() noexcept = default;

        explicit Handle(GlId id) noexcept : id_{id} {
        }

        Handle(const Handle &) = delete;
        auto operator=(const Handle &) -> Handle & = delete;

        Handle(Handle &&other) noexcept : id_{std::exchange(other.id_, 0)} {
        }

        auto operator=(Handle &&other) noexcept -> Handle & {
            if (this == &other) {
                return *this;
            }

            reset();
            id_ = std::exchange(other.id_, 0);

            return *this;
        }

        ~Handle() {
            reset();
        }

        [[nodiscard]] auto get() const noexcept -> GlId { return id_; }

        [[nodiscard]] explicit operator bool() const noexcept { return id_ != 0; }

    private:
        auto reset() noexcept -> void {
            if (id_ != 0) {
                Delete(id_);
                id_ = 0;
            }
        }

        GlId id_{0};
    };
}
