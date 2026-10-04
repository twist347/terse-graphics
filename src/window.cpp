#include "tgx/window.h"

#include "tgx/assert.h"

#include "tgx/gl/version.h"

#include "input_internal.h"
#include "log_internal.h"
#include "window_internal.h"

#include <GLFW/glfw3.h>

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

    auto set_swap_interval(bool vsync) noexcept -> void {
        glfwSwapInterval(vsync ? 1 : 0);
    }

    auto on_glfw_error(int code, const char *desc) noexcept -> void {
        tgx::detail::log_error("glfw {}: {}", code, desc);
    }
}

namespace tgx {
    auto Window::create(const WindowParams &params) noexcept -> Result<Window> {
        TGX_ASSERT_MSG(params.width > 0 && params.height > 0, "window of size {}x{}", params.width, params.height);
        TGX_ASSERT(params.title);

        glfwSetErrorCallback(on_glfw_error);
        if (glfwInit() != GLFW_TRUE) {
            return std::unexpected{Error::platform};
        }

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl::version_major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl::version_minor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        // Required by macOS for any core context; ignored elsewhere, where core
        // profiles have no deprecated functions left to remove anyway.
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, params.debug_context ? GLFW_TRUE : GLFW_FALSE);

        GLFWwindow *handle = glfwCreateWindow(
            params.width, params.height, params.title, nullptr, nullptr
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
        refresh_sizes();
        // The sizes it starts with are not a resize.
        s_window.polled_size = s_window.size;
        s_window.polled_framebuffer_size = s_window.framebuffer_size;
        glfwSetWindowSizeCallback(handle, on_resize);
        glfwSetFramebufferSizeCallback(handle, on_resize);

        detail::attach_input(handle);
        return Window{};
    }

    auto detail::gl_loader() noexcept -> GlLoader {
        return glfwGetProcAddress;
    }

    auto Window::operator=(Window &&other) noexcept -> Window & {
        if (this == &other) {
            return *this;
        }
        destroy();
        m_owned = std::exchange(other.m_owned, false);
        return *this;
    }

    Window::~Window() {
        destroy();
    }

    auto Window::poll_events() noexcept -> void {
        detail::begin_input_frame();
        glfwPollEvents();

        s_window.resized = s_window.size != s_window.polled_size
                           || s_window.framebuffer_size != s_window.polled_framebuffer_size;
        s_window.polled_size = s_window.size;
        s_window.polled_framebuffer_size = s_window.framebuffer_size;
    }

    auto Window::input() const noexcept -> const Input & {
        static const Input input;
        return input;
    }

    auto Window::should_close() const noexcept -> bool {
        return glfwWindowShouldClose(s_window.handle) == GLFW_TRUE;
    }

    auto Window::request_close() noexcept -> void {
        glfwSetWindowShouldClose(s_window.handle, GLFW_TRUE);
    }

    auto detail::swap_buffers() noexcept -> void {
        glfwSwapBuffers(s_window.handle);
    }

    auto Window::set_title(const char *title) noexcept -> void {
        TGX_ASSERT(title);

        glfwSetWindowTitle(s_window.handle, title);
    }

    auto Window::set_vsync(bool enabled) noexcept -> void {
        set_swap_interval(enabled);
    }

    auto Window::resized() const noexcept -> bool {
        return s_window.resized;
    }

    auto detail::window_size() noexcept -> Size {
        return s_window.size;
    }

    auto detail::framebuffer_size() noexcept -> Size {
        return s_window.framebuffer_size;
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

    auto Window::destroy() noexcept -> void {
        if (m_owned) {
            glfwDestroyWindow(s_window.handle);
            glfwTerminate();
            s_window = {};
            m_owned = false;
        }
    }
}
