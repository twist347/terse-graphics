#pragma once

#include "tgx/canvas.h"
#include "tgx/clock.h"
#include "tgx/device.h"
#include "tgx/error.h"
#include "tgx/input.h"
#include "tgx/window.h"

namespace tgx {
    // The simple way in: one window, one device, a canvas and a frame clock, created together and torn down in the right order. The
    // layers underneath stay public for anything this does not cover.
    //
    // App's own methods are the frame loop (should_close, poll_events,
    // swap_buffers) and what it adds on top of its parts (resized,
    // restart_clock). Everything else belongs to a part and is reached through
    // it: app->window(), app->input(), app->canvas(), app->clock(), ... New features come as
    // new parts, not as more methods here.
    class App {
    public:
        [[nodiscard]] static auto create(const WindowParams &params = {}) noexcept -> Result<App>;

        App(App &&) noexcept = default;
        // Member-wise assignment would replace the window before the device
        // that depends on it; with one window per process there is nothing
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
        // Unlike device().present() and window().poll_events(), these also
        // do the per-frame bookkeeping noted on each.
        [[nodiscard]] auto should_close() const noexcept -> bool;

        // Polls events, noting whether the window was resized. The first call
        // starts the clock, so loading done before the loop is not counted in
        // the first delta(). Nothing needs fitting after a resize: draws cover
        // the framebuffer and the canvas follows the window on their own.
        auto poll_events() noexcept -> void;

        // Whether the last poll_events() saw the window or its framebuffer
        // change size; the new sizes are window().size() and
        // window().framebuffer_size().
        [[nodiscard]] auto resized() const noexcept -> bool { return m_resized; }

        // Presents the frame (device().present()), then ticks the clock:
        // delta() is the time from one present to the next.
        auto swap_buffers() noexcept -> void;

        [[nodiscard]] auto window() noexcept -> Window & { return m_window; }
        [[nodiscard]] auto input() const noexcept -> const Input & { return m_window.input(); }
        [[nodiscard]] auto device() noexcept -> Device & { return m_device; }
        [[nodiscard]] auto canvas() noexcept -> Canvas & { return m_canvas; }
        [[nodiscard]] auto clock() const noexcept -> const Clock & { return m_clock; }

        // See Clock::restart: call it after a long pause inside the loop, such
        // as loading a level, so the next delta() does not jump.
        auto restart_clock() noexcept -> void { m_clock.restart(); }

    private:
        App(Window window, Device device, Canvas canvas) noexcept;

        // Declaration order is teardown order reversed: canvas, device, window.
        Window m_window;
        Device m_device;
        Canvas m_canvas;
        Clock m_clock;
        bool m_resized{false};
    };
}
