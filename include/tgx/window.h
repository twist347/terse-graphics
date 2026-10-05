#pragma once

#include "tgx/assert.h"
#include "tgx/error.h"
#include "tgx/input.h"
#include "tgx/math.h"

#include <utility>

struct GLFWwindow;

namespace tgx {
    struct WindowParams {
        int width{1280};
        int height{720};
        const char *title{"tgx"};
        // Whether presenting a frame waits for the display; set_vsync()
        // changes it later.
        bool vsync{true};
        // Debug contexts report driver messages but slow the driver down, so by
        // default only builds with asserts get one. TGX_ENABLE_ASSERTS rather
        // than NDEBUG: it is the same for the library and the app.
        bool debug_context{TGX_ENABLE_ASSERTS != 0};
    };

    // The one window, with its GL context: creating it makes the context
    // current. It also owns the windowing backend (glfwInit/glfwTerminate),
    // which is one per process, so there is one Window, all calls belong to the
    // main thread, and it outlives the Device. Like the Device, it only owns
    // the window; its state is kept in one place inside tgx, so it can be moved
    // freely.
    class Window {
    public:
        [[nodiscard]] static auto create(const WindowParams &params = {}) noexcept -> Result<Window>;

        Window(const Window &) = delete;
        auto operator=(const Window &) -> Window & = delete;

        Window(Window &&other) noexcept : m_owned{std::exchange(other.m_owned, false)} {
        }

        auto operator=(Window &&other) noexcept -> Window &;

        ~Window();

        // Handles what happened since the last call: resizes, close requests,
        // keys and the mouse, which input() then shows.
        auto poll_events() noexcept -> void;

        // Whether the last poll_events() saw the window or its framebuffer
        // change size; the new sizes are size() and framebuffer_size().
        // Nothing needs fitting after it: draws cover the framebuffer and the
        // canvas follows the window on their own.
        [[nodiscard]] auto resized() const noexcept -> bool;

        [[nodiscard]] auto input() const noexcept -> const Input &;

        [[nodiscard]] auto should_close() const noexcept -> bool;

        auto request_close() noexcept -> void;

        // Must be nul-terminated UTF-8, as the windowing backend requires.
        auto set_title(const char *title) noexcept -> void;

        // Whether presenting a frame (Device::present) waits for the display:
        // no tearing, and frames paced by it. Starts as WindowParams::vsync.
        auto set_vsync(bool enabled) noexcept -> void;

        // In screen coordinates, the units the OS lays windows out in and the
        // Canvas draws in. The same as framebuffer_size() unless the display
        // scales, e.g. half of it on a Retina screen. Both sizes are as of the
        // last events polled.
        [[nodiscard]] auto size() const noexcept -> Size;

        // In pixels: what a draw covers unless told otherwise.
        [[nodiscard]] auto framebuffer_size() const noexcept -> Size;

        [[nodiscard]] auto native_handle() const noexcept -> GLFWwindow *;

    private:
        Window() noexcept : m_owned{true} {
        }

        auto destroy() noexcept -> void;

        // False once moved from: the window is someone else's to close.
        bool m_owned{false};
    };
}
