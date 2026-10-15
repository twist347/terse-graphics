#include "tgx/core/window.hpp"

#include "tgx/core/assert.hpp"
#include "tgx/core/version.hpp"

#include "tgx/gl/version.hpp"

#include "core/input_internal.hpp"
#include "core/log_internal.hpp"
#include "core/window_internal.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
    // Sizes are stored by callbacks, not queried on every use.
    struct WindowState {
        GLFWwindow *handle{nullptr};
        tgx::Size size{};
        tgx::Size framebuffer_size{};
        // Compared at the poll, not flagged by the callbacks: GLFW may call
        // those outside a poll (on Wayland, inside glfwSetWindowSize).
        tgx::Size polled_size{};
        tgx::Size polled_framebuffer_size{};
        bool resized{false};
        bool focused{false};
        bool minimized{false};
        // Reapplied after fullscreen changes: some drivers reset the swap
        // interval when the window changes monitor.
        bool vsync{true};
        // Restored when leaving fullscreen.
        int windowed_x{0};
        int windowed_y{0};
        tgx::Size windowed_size{};
    };

    WindowState s_window;

    // Both sizes on either callback: GLFW does not always report both (on
    // Wayland, glfwSetWindowSize reports only the framebuffer). A round trip
    // on X11, but only on a resize.
    auto refresh_sizes() noexcept -> void {
        glfwGetWindowSize(s_window.handle, &s_window.size.width, &s_window.size.height);
        glfwGetFramebufferSize(s_window.handle, &s_window.framebuffer_size.width, &s_window.framebuffer_size.height);
    }

    auto on_resize(GLFWwindow *, int, int) noexcept -> void {
        refresh_sizes();
    }

    auto on_focus(GLFWwindow *, int focused) noexcept -> void {
        s_window.focused = focused == GLFW_TRUE;
    }

    auto on_minimize(GLFWwindow *, int minimized) noexcept -> void {
        s_window.minimized = minimized == GLFW_TRUE;
    }

    auto set_swap_interval(bool vsync) noexcept -> void {
        glfwSwapInterval(vsync ? 1 : 0);
    }

    // Wayland does not let a client get or set its window position; asking
    // makes GLFW report an error.
    [[nodiscard]] auto positions_known() noexcept -> bool {
        return glfwGetPlatform() != GLFW_PLATFORM_WAYLAND;
    }

    // How much [a, a + a_size) and [b, b + b_size) share.
    [[nodiscard]] auto overlap(int a, int a_size, int b, int b_size) noexcept -> long {
        return std::max(0, std::min(a + a_size, b + b_size) - std::max(a, b));
    }

    // The monitor the window covers most, else the primary one.
    [[nodiscard]] auto monitor_of_window() noexcept -> GLFWmonitor * {
        GLFWmonitor *best = glfwGetPrimaryMonitor();
        if (!positions_known()) {
            return best;
        }

        int x = 0, y = 0;
        glfwGetWindowPos(s_window.handle, &x, &y);
        const tgx::Size size = s_window.size;

        int count = 0;
        GLFWmonitor **monitors = glfwGetMonitors(&count);
        long best_area = 0;
        for (GLFWmonitor *monitor: std::span{monitors, static_cast<std::size_t>(count)}) {
            int mx = 0, my = 0;
            glfwGetMonitorPos(monitor, &mx, &my);
            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            if (!mode) {
                continue;
            }
            // All in screen coordinates.
            const long area = overlap(x, size.width, mx, mode->width) * overlap(y, size.height, my, mode->height);
            if (area > best_area) {
                best_area = area;
                best = monitor;
            }
        }
        return best;
    }

    // Covers the monitor the window is mostly on, at its current mode: no
    // mode switch, just borderless. False if there is no monitor to cover.
    auto enter_fullscreen() noexcept -> bool {
        // A minimized window has no size to go back to (0x0 on Windows).
        if (s_window.minimized) {
            glfwRestoreWindow(s_window.handle);
            refresh_sizes();
        }
        if (positions_known()) {
            glfwGetWindowPos(s_window.handle, &s_window.windowed_x, &s_window.windowed_y);
        }
        s_window.windowed_size = s_window.size;

        GLFWmonitor *monitor = monitor_of_window();
        const GLFWvidmode *mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (!mode) {
            return false;
        }
        glfwSetWindowMonitor(s_window.handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        return true;
    }

    // Back to the size and position before fullscreen. Wayland ignores the
    // position; the compositor places the window.
    auto leave_fullscreen() noexcept -> void {
        glfwSetWindowMonitor(
            s_window.handle,
            nullptr,
            s_window.windowed_x,
            s_window.windowed_y,
            s_window.windowed_size.width,
            s_window.windowed_size.height,
            GLFW_DONT_CARE
        );
    }

    // The hints are hard requirements: no older context. Creating also fails
    // with no OpenGL at all or no pixel format with a depth buffer.
    [[nodiscard]] auto create_error() noexcept -> tgx::Error {
        switch (glfwGetError(nullptr)) {
            case GLFW_VERSION_UNAVAILABLE:
            case GLFW_API_UNAVAILABLE:
            case GLFW_FORMAT_UNAVAILABLE: return tgx::Error::unsupported;
            default: return tgx::Error::platform;
        }
    }

    auto on_glfw_error(int code, const char *desc) noexcept -> void {
        tgx::detail::log_error("glfw {}: {}", code, desc);
    }

    [[nodiscard]] auto platform_name() noexcept -> std::string_view {
        switch (glfwGetPlatform()) {
            case GLFW_PLATFORM_WIN32: return "Win32";
            case GLFW_PLATFORM_COCOA: return "Cocoa";
            case GLFW_PLATFORM_WAYLAND: return "Wayland";
            case GLFW_PLATFORM_X11: return "X11";
            default: return "another platform";
        }
    }

    // What a bug report needs first: version, windowing system, sizes (a
    // scaled display makes the framebuffer larger), monitor, and the
    // directory relative paths start from.
    auto log_window_info() noexcept -> void {
        if (!tgx::detail::log_enabled(tgx::LogLevel::info)) {
            return;
        }
        int major = 0, minor = 0, revision = 0;
        glfwGetVersion(&major, &minor, &revision);
        tgx::detail::log_info("tgx {}, GLFW {}.{}.{} on {}", TGX_VERSION_STRING, major, minor, revision, platform_name());
        tgx::detail::log_info(
            "  window:   {}x{}, framebuffer {}x{}, vsync {}",
            s_window.size.width, s_window.size.height,
            s_window.framebuffer_size.width, s_window.framebuffer_size.height,
            s_window.vsync ? "on" : "off"
        );
        if (GLFWmonitor *monitor = glfwGetPrimaryMonitor()) {
            if (const GLFWvidmode *mode = glfwGetVideoMode(monitor)) {
                const char *name = glfwGetMonitorName(monitor);
                tgx::detail::log_info(
                    "  monitor:  {}, {}x{} at {} Hz",
                    name ? name : "unnamed", mode->width, mode->height, mode->refreshRate
                );
            }
        }
        std::error_code err;
        const std::filesystem::path cwd = std::filesystem::current_path(err);
        if (!err) {
            const std::u8string utf8 = cwd.u8string();
            tgx::detail::log_info("  cwd:      {}", std::string_view{reinterpret_cast<const char *>(utf8.data()), utf8.size()});
        }
    }
}

namespace tgx {
    auto Window::create(const WindowParams &params) noexcept -> Result<Window> {
        TGX_ASSERT_MSG(!params.size.empty(), "window of size {}x{}", params.size.width, params.size.height);
        TGX_ASSERT(params.title);

        glfwSetErrorCallback(on_glfw_error);
        if (glfwInit() != GLFW_TRUE) {
            return std::unexpected{Error::platform};
        }

        glfwDefaultWindowHints();
        // Explicit, not GLFW's defaults. Window draws may use depth testing
        // (Device asserts it only for render targets), and raw GL a stencil.
        glfwWindowHint(GLFW_DEPTH_BITS, 24);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl::version_major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl::version_minor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        // macOS requires it for any core context. Ignored elsewhere.
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, params.debug_context ? GLFW_TRUE : GLFW_FALSE);
        // GLFW_AUTO_ICONIFY stays on. GLFW keeps a fullscreen window on top
        // (Windows, macOS) until it minimizes; otherwise alt-tab would leave
        // it covering the monitor.

        GLFWwindow *handle = glfwCreateWindow(
            params.size.width, params.size.height, params.title, nullptr, nullptr
        );
        if (!handle) {
            const Error error = create_error();
            glfwTerminate();
            return std::unexpected{error};
        }

        glfwMakeContextCurrent(handle);
        set_swap_interval(params.vsync);

        s_window.handle = handle;
        s_window.vsync = params.vsync;
        refresh_sizes();
        // Starting sizes are not a resize.
        s_window.polled_size = s_window.size;
        s_window.polled_framebuffer_size = s_window.framebuffer_size;
        glfwSetWindowSizeCallback(handle, on_resize);
        glfwSetFramebufferSizeCallback(handle, on_resize);
        // From here the callbacks update these.
        s_window.focused = glfwGetWindowAttrib(handle, GLFW_FOCUSED) == GLFW_TRUE;
        s_window.minimized = glfwGetWindowAttrib(handle, GLFW_ICONIFIED) == GLFW_TRUE;
        glfwSetWindowFocusCallback(handle, on_focus);
        glfwSetWindowIconifyCallback(handle, on_minimize);

        detail::attach_input(handle);
        log_window_info();
        return Window{};
    }

    auto Window::operator=(Window &&other) noexcept -> Window & {
        if (this == &other) {
            return *this;
        }
        release();
        m_owned = std::exchange(other.m_owned, false);
        return *this;
    }

    Window::~Window() {
        release();
    }

    auto Window::poll_events() noexcept -> void {
        detail::begin_input_frame();
        glfwPollEvents();

        s_window.resized =
            s_window.size != s_window.polled_size || s_window.framebuffer_size != s_window.polled_framebuffer_size;
        s_window.polled_size = s_window.size;
        s_window.polled_framebuffer_size = s_window.framebuffer_size;
    }

    auto Window::resized() const noexcept -> bool {
        return s_window.resized;
    }

    auto Window::input() const noexcept -> const Input & {
        static const Input input;
        return input;
    }

    auto Window::focused() const noexcept -> bool {
        return s_window.focused;
    }

    auto Window::minimized() const noexcept -> bool {
        return s_window.minimized;
    }

    auto Window::should_close() const noexcept -> bool {
        return glfwWindowShouldClose(s_window.handle) == GLFW_TRUE;
    }

    auto Window::request_close() noexcept -> void {
        glfwSetWindowShouldClose(s_window.handle, GLFW_TRUE);
    }

    auto Window::set_title(const char *title) noexcept -> void {
        TGX_ASSERT(title);

        glfwSetWindowTitle(s_window.handle, title);
    }

    auto Window::set_vsync(bool enabled) noexcept -> void {
        s_window.vsync = enabled;
        set_swap_interval(enabled);
    }

    auto Window::vsync() const noexcept -> bool {
        return s_window.vsync;
    }

    auto Window::set_fullscreen(bool fullscreen) noexcept -> void {
        if (fullscreen == this->fullscreen()) {
            return;
        }

        if (fullscreen) {
            if (!enter_fullscreen()) {
                return;
            }
        } else {
            leave_fullscreen();
        }
        set_swap_interval(s_window.vsync);
    }

    // Queried, not stored: one source of truth.
    auto Window::fullscreen() const noexcept -> bool {
        return static_cast<bool>(glfwGetWindowMonitor(s_window.handle));
    }

    auto Window::size() const noexcept -> Size {
        return s_window.size;
    }

    auto Window::framebuffer_size() const noexcept -> Size {
        return s_window.framebuffer_size;
    }

    auto Window::native_handle() const noexcept -> GLFWwindow * {
        return s_window.handle;
    }

    auto Window::release() noexcept -> void {
        if (m_owned) {
            detail::detach_input();
            glfwDestroyWindow(s_window.handle);
            glfwTerminate();
            s_window = {};
            m_owned = false;
        }
    }

    auto detail::gl_loader() noexcept -> GlLoader {
        return glfwGetProcAddress;
    }

    auto detail::swap_buffers() noexcept -> void {
        glfwSwapBuffers(s_window.handle);
    }

    auto detail::window_size() noexcept -> Size {
        return s_window.size;
    }

    auto detail::framebuffer_size() noexcept -> Size {
        return s_window.framebuffer_size;
    }
}
