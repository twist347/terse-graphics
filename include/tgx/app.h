#pragma once

#include "tgx/clock.h"
#include "tgx/device.h"
#include "tgx/error.h"
#include "tgx/platform.h"
#include "tgx/window.h"

namespace tgx {
    // The simple way in: one platform, one window, one device and a frame clock,
    // created together and torn down in the right order. The layers underneath
    // stay public for anything this does not cover.
    class App {
    public:
        [[nodiscard]] static auto create(const WindowParams &params = {}) noexcept -> Result<App>;

        App(App &&) noexcept = default;
        // Member-wise assignment would replace the platform before the window
        // that depends on it; with one platform per process there is nothing
        // useful to assign anyway.
        auto operator=(App &&) noexcept -> App & = delete;

        App(const App &) = delete;
        auto operator=(const App &) -> App & = delete;

        // Finishes the previous frame and starts the next one: presents what was
        // drawn, polls events, ticks the clock and fits the viewport to the
        // window. Returns false once the window has been asked to close.
        [[nodiscard]] auto next_frame() noexcept -> bool;

        [[nodiscard]] auto platform() noexcept -> Platform & { return m_platform; }
        [[nodiscard]] auto window() noexcept -> Window & { return m_window; }
        [[nodiscard]] auto device() noexcept -> Device & { return m_device; }
        [[nodiscard]] auto clock() const noexcept -> const Clock & { return m_clock; }

    private:
        App(Platform platform, Window window, Device device) noexcept;

        // Declaration order is teardown order reversed: device, window, platform.
        Platform m_platform;
        Window m_window;
        Device m_device;
        Clock m_clock;
        bool m_in_frame{false};
    };
}
