#pragma once

#include "tgx/error.h"

#include <utility>

namespace tgx {
    // Owns the windowing backend (glfwInit/glfwTerminate), which is one per
    // process: there is one Platform, it outlives the Window, and all calls
    // belong to the main thread.
    class Platform {
    public:
        [[nodiscard]] static auto create() noexcept -> Result<Platform>;

        Platform(const Platform &) = delete;
        auto operator=(const Platform &) -> Platform & = delete;

        Platform(Platform &&other) noexcept : m_owned{std::exchange(other.m_owned, false)} {
        }

        auto operator=(Platform &&other) noexcept -> Platform &;

        ~Platform();

        auto poll_events() noexcept -> void;

    private:
        Platform() noexcept : m_owned{true} {
        }

        auto shutdown() noexcept -> void;

        // False once moved from: the backend is someone else's to shut down.
        bool m_owned{false};
    };
}
