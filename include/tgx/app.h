#pragma once

#include "tgx/clock.h"
#include "tgx/device.h"
#include "tgx/error.h"
#include "tgx/platform.h"
#include "tgx/size.h"
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

        // The frame loop, in the same shape as a plain GLFW one:
        //
        //     while (!app->should_close()) {
        //         app->poll_events();
        //         ... draw ...
        //         app->swap_buffers();
        //     }
        //
        // Unlike window().swap_buffers() and platform().poll_events(), these
        // also do the per-frame bookkeeping noted on each.
        [[nodiscard]] auto should_close() const noexcept -> bool;

        // Polls events, then fits the viewport to the window if its size
        // changed. A viewport set by hand stays until the next resize. The
        // first call starts the clock, so loading done before the loop is not
        // counted in the first delta().
        auto poll_events() noexcept -> void;

        // Whether the last poll_events() saw the window's framebuffer change
        // size; the new size is window().framebuffer_size().
        [[nodiscard]] auto resized() const noexcept -> bool { return m_resized; }

        // Presents the frame, then ticks the clock: delta() is the time from
        // one present to the next.
        auto swap_buffers() noexcept -> void;

        [[nodiscard]] auto platform() noexcept -> Platform & { return m_platform; }
        [[nodiscard]] auto window() noexcept -> Window & { return m_window; }
        [[nodiscard]] auto device() noexcept -> Device & { return m_device; }
        [[nodiscard]] auto clock() const noexcept -> const Clock & { return m_clock; }

        // See Clock::restart: call it after a long pause inside the loop, such
        // as loading a level, so the next delta() does not jump.
        auto restart_clock() noexcept -> void { m_clock.restart(); }

    private:
        App(Platform platform, Window window, Device device) noexcept;

        // Declaration order is teardown order reversed: device, window, platform.
        Platform m_platform;
        Window m_window;
        Device m_device;
        Clock m_clock;
        // What the viewport was last fitted to, to notice a resize.
        Size m_framebuffer_size{};
        bool m_resized{false};
    };
}
