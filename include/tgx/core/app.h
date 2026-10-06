#pragma once

#include "tgx/core/audio.h"
#include "tgx/core/canvas.h"
#include "tgx/core/clock.h"
#include "tgx/core/device.h"
#include "tgx/core/error.h"
#include "tgx/core/input.h"
#include "tgx/core/window.h"

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
        // Fails as Window::create, then Device::create do.
        [[nodiscard]] static auto create(const WindowParams &params = {}) -> Result<App>;

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

        // The parts; const for a const App (deducing this: one method for both).
        [[nodiscard]] auto window(this auto &self) noexcept -> auto & { return self.m_window; }
        [[nodiscard]] auto input() const noexcept -> const Input & { return m_window.input(); }
        [[nodiscard]] auto device(this auto &self) noexcept -> auto & { return self.m_device; }
        [[nodiscard]] auto canvas(this auto &self) noexcept -> auto & { return self.m_canvas; }
        [[nodiscard]] auto clock() const noexcept -> const Clock & { return m_device.clock(); }
        [[nodiscard]] auto audio(this auto &self) noexcept -> auto & { return self.m_audio; }

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
