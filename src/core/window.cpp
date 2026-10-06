#include "tgx/core/window.h"

#include "tgx/core/assert.h"
#include "tgx/core/version.h"

#include "tgx/gl/version.h"

#include "core/input_internal.h"
#include "core/log_internal.h"
#include "core/window_internal.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
    // The one window: its handle, and its sizes as GLFW last reported them,
    // kept by callbacks rather than asked for on every use.
    struct WindowState {
        GLFWwindow *handle{nullptr};
        tgx::Size size{};
        tgx::Size framebuffer_size{};
        // Both sizes as the poll before the last one left them, and whether
        // the last one left them different. Compared at the poll rather than
        // flagged by the callbacks, which GLFW may also call outside a poll
        // (on Wayland, from inside glfwSetWindowSize).
        tgx::Size polled_size{};
        tgx::Size polled_framebuffer_size{};
        bool resized{false};
        bool focused{false};
        bool minimized{false};
        // Set again after leaving or entering fullscreen: some drivers reset
        // the swap interval when the window changes monitor.
        bool vsync{true};
        // Where the window was and how big, to go back to from fullscreen.
        int windowed_x{0};
        int windowed_y{0};
        tgx::Size windowed_size{};
    };

    WindowState s_window;

    // Both sizes on either callback: GLFW does not always report both (on
    // Wayland, glfwSetWindowSize reports only the framebuffer). Asking is a
    // round trip on X11, but only on a resize.
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

    // Wayland lets no client know or set where its window is: asking makes
    // GLFW report an error.
    [[nodiscard]] auto positions_known() noexcept -> bool {
        return glfwGetPlatform() != GLFW_PLATFORM_WAYLAND;
    }

    // The monitor the window covers the most of, the primary one when that
    // cannot be told.
    [[nodiscard]] auto monitor_of_window() noexcept -> GLFWmonitor * {
        GLFWmonitor *best = glfwGetPrimaryMonitor();
        if (!positions_known()) {
            return best;
        }

        int x = 0;
        int y = 0;
        glfwGetWindowPos(s_window.handle, &x, &y);
        const tgx::Size size = s_window.size;

        int count = 0;
        GLFWmonitor **monitors = glfwGetMonitors(&count);
        long best_area = 0;
        for (int i = 0; i < count; ++i) {
            int mx = 0;
            int my = 0;
            glfwGetMonitorPos(monitors[i], &mx, &my);
            const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);
            if (!mode) {
                continue;
            }
            // Monitor positions and video modes are in screen coordinates,
            // like the window's.
            const long w = std::max(0, std::min(x + size.width, mx + mode->width) - std::max(x, mx));
            const long h = std::max(0, std::min(y + size.height, my + mode->height) - std::max(y, my));
            if (w * h > best_area) {
                best_area = w * h;
                best = monitors[i];
            }
        }
        return best;
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

    // What a bug report needs first: which tgx, on which windowing system,
    // the sizes the window got (a scaled display makes the framebuffer
    // larger), the monitor, and where relative paths start from.
    auto log_window_info() noexcept -> void {
        if (!tgx::detail::log_enabled(tgx::LogLevel::info)) {
            return;
        }
        int major = 0;
        int minor = 0;
        int revision = 0;
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
        // Draws into the window may test depth and clear stencil (Device
        // asserts only render targets): asked for rather than left to GLFW's
        // defaults.
        glfwWindowHint(GLFW_DEPTH_BITS, 24);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl::version_major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl::version_minor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        // Required by macOS for any core context; ignored elsewhere, where core
        // profiles have no deprecated functions left to remove anyway.
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, params.debug_context ? GLFW_TRUE : GLFW_FALSE);
        // GLFW_AUTO_ICONIFY stays on: a fullscreen window minimizes when it
        // loses focus. GLFW keeps a fullscreen window above all others
        // (Windows, macOS) and lowers it only when it minimizes, so without
        // that alt-tab would leave the game covering the monitor.

        GLFWwindow *handle = glfwCreateWindow(
            params.size.width, params.size.height, params.title, nullptr, nullptr
        );
        if (!handle) {
            // The hints above are a hard requirement: GLFW refuses rather than
            // hand out an older context.
            const Error error = glfwGetError(nullptr) == GLFW_VERSION_UNAVAILABLE
                                    ? Error::unsupported
                                    : Error::platform;
            glfwTerminate();
            return std::unexpected{error};
        }

        glfwMakeContextCurrent(handle);
        set_swap_interval(params.vsync);

        s_window.handle = handle;
        s_window.vsync = params.vsync;
        refresh_sizes();
        // The sizes it starts with are not a resize.
        s_window.polled_size = s_window.size;
        s_window.polled_framebuffer_size = s_window.framebuffer_size;
        glfwSetWindowSizeCallback(handle, on_resize);
        glfwSetFramebufferSizeCallback(handle, on_resize);
        // As it starts; the callbacks keep both up to date from here.
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

        s_window.resized = s_window.size != s_window.polled_size
                           || s_window.framebuffer_size != s_window.polled_framebuffer_size;
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
                return;
            }
            // The monitor's own mode: GLFW then switches nothing, the window
            // only loses its border and covers it.
            glfwSetWindowMonitor(s_window.handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            // On Wayland the position is ignored: the compositor places it.
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
        set_swap_interval(s_window.vsync);
    }

    // Asked of GLFW rather than kept alongside it: one place to be true.
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
