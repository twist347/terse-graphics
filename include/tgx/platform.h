#pragma once

#include "tgx/error.h"

namespace tgx {
    // Owns the windowing backend (glfwInit/glfwTerminate). At most one may exist,
    // it must outlive every Window, and all calls belong to the main thread.
    class Platform {
    public:
        [[nodiscard]] static auto create() noexcept -> Result<Platform>;

        // Pinned: windows keep a pointer back to their platform.
        Platform(const Platform &) = delete;
        auto operator=(const Platform &) -> Platform & = delete;

        Platform(Platform &&other) noexcept = delete;
        auto operator=(Platform &&other) noexcept -> Platform & = delete;

        ~Platform();

        void poll_events() noexcept;

    private:
        friend class Window;

        Platform() noexcept = default;

        int m_window_count{0};
    };
}
