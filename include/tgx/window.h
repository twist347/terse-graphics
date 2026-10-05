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

        // Whether keyboard input goes to the window: false once the player
        // switches to another, the usual sign to pause a game. Keys and
        // buttons held when it loses focus are let go, so none stays down.
        // As of the last poll_events().
        [[nodiscard]] auto focused() const noexcept -> bool;

        // Whether it is minimized to the taskbar or the dock. Always false on
        // Wayland, which does not tell a window it was minimized: focused()
        // is what to pause on there, and works everywhere.
        [[nodiscard]] auto minimized() const noexcept -> bool;

        [[nodiscard]] auto should_close() const noexcept -> bool;

        auto request_close() noexcept -> void;

        // Must be nul-terminated UTF-8, as the windowing backend requires.
        auto set_title(const char *title) noexcept -> void;

        // Whether presenting a frame (Device::present) waits for the display:
        // no tearing, and frames paced by it. Starts as WindowParams::vsync.
        auto set_vsync(bool enabled) noexcept -> void;

        // Covers a whole monitor, without a border, at the resolution the
        // monitor already has (its video mode is left alone), or goes back to
        // the size and place the window had. The monitor is the one the
        // window is most on; on Wayland, which keeps window positions to
        // itself, the primary one. The sizes change as on any resize
        // (resized(), size()): the Canvas follows, render targets are yours
        // to remake.
        auto set_fullscreen(bool fullscreen) noexcept -> void;

        [[nodiscard]] auto fullscreen() const noexcept -> bool;

        // In screen coordinates, the units the OS lays windows out in and the
        // Canvas draws in. The same as framebuffer_size() unless the display
        // scales, e.g. half of it on a Retina screen. Both sizes are as of the
        // last events polled.
        [[nodiscard]] auto size() const noexcept -> Size;

        // In pixels: what a draw into the window covers unless told otherwise.
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
