#pragma once

#include "tgx/error.h"

namespace tgx {
    // Owns the windowing backend (glfwInit/glfwTerminate). At most one may exist,
    // it must outlive every Window, and all calls belong to the main thread.
    class Platform {
    public:
        [[nodiscard]] static auto create() noexcept -> Result<Platform>;

        Platform(const Platform &) = delete;
        auto operator=(const Platform &) -> Platform & = delete;

        Platform(Platform &&other) noexcept;
        auto operator=(Platform &&other) noexcept -> Platform &;

        ~Platform();

        auto poll_events() noexcept -> void;

        // False only for a moved-from platform.
        [[nodiscard]] auto is_valid() const noexcept -> bool { return m_owned; }

    private:
        Platform() noexcept : m_owned{true} {
        }

        auto shutdown() noexcept -> void;

        bool m_owned{false};
    };
}
