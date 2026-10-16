#pragma once

#include "tgx/core/error.hpp"
#include "tgx/core/input.hpp"
#include "tgx/core/math.hpp"

#include <utility>

struct GLFWwindow;

namespace tgx {
    struct WindowParams {
        // In screen coordinates, as Window::size().
        Size size{.width = 1280, .height = 720};
        const char *title{"tgx"};
        bool vsync{true};
        int fps_limit{0};
        // Samples per pixel, for smooth edges of turned and round shapes: 0
        // for none, 4 the usual. The window's only, not render targets'. The
        // driver may give another count; the Device logs it.
        int msaa{0};
        // Reports driver messages but slows the driver: on by default only
        // with asserts. TGX_ENABLE_ASSERTS, unlike NDEBUG, is the same for the
        // library and the app.
        bool debug_context{TGX_ENABLE_ASSERTS != 0};
    };

    // The one window and its GL context, made current on create. It also owns
    // the windowing backend (glfwInit/glfwTerminate), one per process. So: one
    // Window, all calls on the main thread, and it outlives the Device. Its
    // state lives inside tgx, so it moves freely.
    class Window {
    public:
        // Fails with Error::unsupported without OpenGL 3.3 core, and with
        // Error::platform when the windowing system fails otherwise.
        [[nodiscard]] static auto create(const WindowParams &params = {}) noexcept -> Result<Window>;

        Window(const Window &) = delete;
        auto operator=(const Window &) -> Window & = delete;

        Window(Window &&other) noexcept : owned_{std::exchange(other.owned_, false)} {
        }

        auto operator=(Window &&other) noexcept -> Window &;

        ~Window();

        // Resizes, close requests, keys and the mouse since the last call.
        // Updates input().
        auto poll_events() noexcept -> void;

        // Whether either size changed in the last poll_events(). Nothing needs
        // adjusting: draws cover the framebuffer and the canvas follows.
        [[nodiscard]] auto resized() const noexcept -> bool;

        [[nodiscard]] auto input() const noexcept -> const Input &;

        // False once the player switches to another window: the usual sign
        // to pause. Losing focus releases held keys and buttons. As of the
        // last poll_events().
        [[nodiscard]] auto focused() const noexcept -> bool;

        // Always false on Wayland, which does not report it. Pause on
        // focused() instead: it works everywhere.
        [[nodiscard]] auto minimized() const noexcept -> bool;

        [[nodiscard]] auto should_close() const noexcept -> bool;

        auto request_close() noexcept -> void;

        // Nul-terminated UTF-8.
        auto set_title(const char *title) noexcept -> void;

        // Whether Device::present waits for the display: no tearing, frames
        // paced to the display.
        auto set_vsync(bool enabled) noexcept -> void;

        [[nodiscard]] auto vsync() const noexcept -> bool;

        // Frames a second at most, 0 for none: Device::present waits out the
        // rest of the frame. For when vsync is off, or to spare the battery.
        // Under the display's rate with vsync on, frames come unevenly.
        auto set_fps_limit(int fps) noexcept -> void;

        [[nodiscard]] auto fps_limit() const noexcept -> int;

        // Borderless over a whole monitor, no video mode change; or back to
        // the old size and position. The monitor the window is mostly on (on
        // Wayland, the primary one). A resize: the Canvas follows, but
        // recreate your render targets.
        auto set_fullscreen(bool fullscreen) noexcept -> void;

        [[nodiscard]] auto fullscreen() const noexcept -> bool;

        // In screen coordinates: the OS layout units the Canvas draws in.
        // Equals framebuffer_size() unless the display scales, e.g. half of it
        // on Retina. Both as of the last poll_events().
        [[nodiscard]] auto size() const noexcept -> Size;

        // In pixels: what a draw into the window covers by default.
        [[nodiscard]] auto framebuffer_size() const noexcept -> Size;

        // For what tgx does not do, with <GLFW/glfw3.h> (tgx ships it with
        // its own GLFW copy). tgx sets the callbacks: replacing them
        // with glfwSet*Callback stops input(), resized(), focused() and
        // minimized() from updating.
        [[nodiscard]] auto native_handle() const noexcept -> GLFWwindow *;

    private:
        Window() noexcept : owned_{true} {
        }

        auto release() noexcept -> void;

        // False once moved from: another Window closes the window.
        bool owned_{false};
    };
}
