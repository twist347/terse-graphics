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

    // The claim on a one-per-process part whose state lives in a global (the
    // window, the GL context, the audio output): calls Release once, from
    // whichever holds it last. Moves but does not assign: the new claim is on
    // the same global, so releasing the old one would end it. Not API.
    template<void (*Release)() noexcept>
    class Claim {
    public:
        Claim() noexcept = default;

        explicit Claim(bool held) noexcept : held_{held} {
        }

        Claim(const Claim &) = delete;
        auto operator=(const Claim &) -> Claim & = delete;

        Claim(Claim &&other) noexcept : held_{std::exchange(other.held_, false)} {
        }

        auto operator=(Claim &&) noexcept -> Claim & = delete;

        ~Claim() {
            if (held_) {
                Release();
            }
        }

    private:
        bool held_{false};
    };
}
