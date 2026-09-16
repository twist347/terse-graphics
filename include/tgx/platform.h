#pragma once

#include "tgx/error.h"

namespace tgx {
    class Platform {
    public:
        [[nodiscard]] static auto create() noexcept -> Result<Platform>;

        Platform(const Platform &) = delete;

        auto operator=(const Platform &) -> Platform & = delete;

        Platform(Platform &&other) noexcept;

        auto operator=(Platform &&other) noexcept -> Platform &;

        void poll_events() noexcept;

        [[nodiscard]] auto is_valid() const noexcept -> bool { return m_owned; }

        ~Platform();

    private:
        Platform() noexcept : m_owned{true} {
        }

        auto shutdown() noexcept -> void;

        bool m_owned = false;
    };
}
