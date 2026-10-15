#pragma once

#include "tgx/core/audio.hpp"
#include "tgx/core/canvas.hpp"
#include "tgx/core/clock.hpp"
#include "tgx/core/device.hpp"
#include "tgx/core/error.hpp"
#include "tgx/core/input.hpp"
#include "tgx/core/window.hpp"

namespace tgx {
    // The easy way to start: creates and owns the window, device, a canvas
    // and audio, all or nothing. Without an audio output device it stays
    // silent instead of failing. The lower layers stay public.
    //
    // Its own methods are the frame loop; the rest is reached through parts:
    // app->window().resized(), app->canvas(), ... New features become new
    // parts, not methods here.
    class App {
    public:
        // Fails the way Window::create, then Device::create, fail.
        [[nodiscard]] static auto create(const WindowParams &params = {}) -> Result<App>;

        App(App &&) noexcept = default;
        // Would replace the window before the device that depends on it.
        // Useless with one window per process anyway.
        auto operator=(App &&) noexcept -> App & = delete;

        App(const App &) = delete;
        auto operator=(const App &) -> App & = delete;

        // The frame loop, like a plain GLFW one:
        //
        //     while (!app->should_close()) {
        //         app->poll_events();
        //         ... draw ...
        //         app->swap_buffers();
        //     }
        //
        // Same as window().should_close(), window().poll_events() and
        // device().present().
        [[nodiscard]] auto should_close() const noexcept -> bool;

        auto poll_events() noexcept -> void;

        auto swap_buffers() noexcept -> void;

        [[nodiscard]] auto window(this auto &self) noexcept -> auto & { return self.window_; }
        [[nodiscard]] auto input() const noexcept -> const Input & { return window_.input(); }
        [[nodiscard]] auto device(this auto &self) noexcept -> auto & { return self.device_; }
        [[nodiscard]] auto canvas(this auto &self) noexcept -> auto & { return self.canvas_; }
        [[nodiscard]] auto clock() const noexcept -> const Clock & { return device_.clock(); }
        [[nodiscard]] auto audio(this auto &self) noexcept -> auto & { return self.audio_; }

    private:
        App(Window window, Device device, Canvas canvas, Audio audio) noexcept;

        // The order of destruction matters.
        Window window_;
        Device device_;
        Canvas canvas_;
        Audio audio_;
    };
}
