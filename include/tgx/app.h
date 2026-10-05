#pragma once

#include "tgx/audio.h"
#include "tgx/canvas.h"
#include "tgx/clock.h"
#include "tgx/device.h"
#include "tgx/error.h"
#include "tgx/input.h"
#include "tgx/window.h"

namespace tgx {
    // The simple way in: creates the window, the device, a canvas and the
    // audio in the right order, failing as one call, owns them, tears them
    // down in the reverse order, and gives the frame loop in three words.
    // The audio never fails it: without an output device it stays silent.
    // The layers underneath stay public for anything this does not cover.
    //
    // App's own methods are the frame loop (should_close, poll_events,
    // swap_buffers). Everything else belongs to a part and is reached through
    // it: app->window().resized(), app->input(), app->canvas(),
    // app->clock(), app->audio(), ... New features come as new parts, not as
    // more methods here.
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
        // The same as window().should_close(), window().poll_events() and
        // device().present().
        [[nodiscard]] auto should_close() const noexcept -> bool;

        auto poll_events() noexcept -> void;

        auto swap_buffers() noexcept -> void;

        [[nodiscard]] auto window() noexcept -> Window & { return m_window; }
        [[nodiscard]] auto window() const noexcept -> const Window & { return m_window; }
        [[nodiscard]] auto input() const noexcept -> const Input & { return m_window.input(); }
        [[nodiscard]] auto device() noexcept -> Device & { return m_device; }
        [[nodiscard]] auto canvas() noexcept -> Canvas & { return m_canvas; }
        [[nodiscard]] auto clock() const noexcept -> const Clock & { return m_device.clock(); }
        [[nodiscard]] auto audio() noexcept -> Audio & { return m_audio; }

    private:
        App(Window window, Device device, Canvas canvas, Audio audio) noexcept;

        // Declaration order is teardown order reversed: audio, canvas,
        // device, window.
        Window m_window;
        Device m_device;
        Canvas m_canvas;
        Audio m_audio;
    };
}
