#include "tgx/window.h"

#include "tgx/assert.h"
#include "tgx/platform.h"

#include "tgx/gl/version.h"

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
}

namespace tgx {
    auto Window::create(Platform &, const WindowParams &params) noexcept -> Result<Window> {
        TGX_ASSERT_MSG(params.width > 0 && params.height > 0, "window of size {}x{}", params.width, params.height);
        TGX_ASSERT(params.title);

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
            if (glfwGetError(nullptr) == GLFW_VERSION_UNAVAILABLE) {
                return std::unexpected{Error::unsupported};
            }
            return std::unexpected{Error::platform};
        }

        glfwMakeContextCurrent(handle);
        detail::set_vsync(params.vsync);

        s_window.handle = handle;
        refresh_sizes();
        glfwSetWindowSizeCallback(handle, on_resize);
        glfwSetFramebufferSizeCallback(handle, on_resize);
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

    auto detail::set_vsync(bool enabled) noexcept -> void {
        glfwSwapInterval(enabled ? 1 : 0);
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
            s_window = {};
            m_owned = false;
        }
    }
}
