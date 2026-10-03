#pragma once

#include "tgx/canvas.h"
#include "tgx/clock.h"
#include "tgx/error.h"
#include "tgx/platform.h"
#include "tgx/size.h"
#include "tgx/window.h"

#include "tgx/gl/device.h"

namespace tgx {
    // The simple way in: one platform, one window, one device, a canvas and a
    // frame clock, created together and torn down in the right order. The
    // layers underneath stay public for anything this does not cover.
    //
    // App's own methods are the frame loop (should_close, poll_events,
    // swap_buffers) and what it adds on top of its parts (resized,
    // restart_clock). Everything else belongs to a part and is reached through
    // it: app->window(), app->canvas(), app->clock(), ... New features come as
    // new parts, not as more methods here.
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

        // Polls events, then fits the viewport and the canvas to the window if
        // its size changed. A viewport set by hand stays until the next resize. The
        // first call starts the clock, so loading done before the loop is not
        // counted in the first delta().
        auto poll_events() noexcept -> void;

        // Whether the last poll_events() saw the window or its framebuffer
        // change size; the new sizes are window().size() and
        // window().framebuffer_size().
        [[nodiscard]] auto resized() const noexcept -> bool { return m_resized; }

        // Draws what the canvas still holds, presents the frame, then ticks the
        // clock: delta() is the time from one present to the next.
        auto swap_buffers() noexcept -> void;

        [[nodiscard]] auto platform() noexcept -> Platform & { return m_platform; }
        [[nodiscard]] auto window() noexcept -> Window & { return m_window; }
        [[nodiscard]] auto device() noexcept -> gl::Device & { return m_device; }
        [[nodiscard]] auto canvas() noexcept -> Canvas & { return m_canvas; }
        [[nodiscard]] auto clock() const noexcept -> const Clock & { return m_clock; }

        // See Clock::restart: call it after a long pause inside the loop, such
        // as loading a level, so the next delta() does not jump.
        auto restart_clock() noexcept -> void { m_clock.restart(); }

    private:
        App(Platform platform, Window window, gl::Device device, Canvas canvas) noexcept;

        // Declaration order is teardown order reversed: canvas, device, window,
        // platform.
        Platform m_platform;
        Window m_window;
        gl::Device m_device;
        Canvas m_canvas;
        Clock m_clock;
        // What the viewport and the canvas were last fitted to, to notice a
        // resize.
        Size m_framebuffer_size{};
        Size m_window_size{};
        bool m_resized{false};
    };
}
