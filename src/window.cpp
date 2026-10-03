#include "tgx/window.h"

#include "tgx/assert.h"
#include "tgx/platform.h"

#include "tgx/gl/version.h"

#include "window_internal.h"

#include <GLFW/glfw3.h>

#include <utility>

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

        Window window{handle};
        detail::set_vsync(params.vsync);
        return window;
    }

    auto detail::gl_loader(const Window &) noexcept -> GlLoader {
        return glfwGetProcAddress;
    }

    Window::Window(GLFWwindow *handle) noexcept : m_handle{handle} {
    }

    Window::Window(Window &&other) noexcept : m_handle{std::exchange(other.m_handle, nullptr)} {
    }

    auto Window::operator=(Window &&other) noexcept -> Window & {
        if (this == &other) {
            return *this;
        }
        destroy();
        m_handle = std::exchange(other.m_handle, nullptr);
        return *this;
    }

    Window::~Window() {
        destroy();
    }

    auto Window::should_close() const noexcept -> bool {
        return glfwWindowShouldClose(m_handle) == GLFW_TRUE;
    }

    auto Window::request_close() noexcept -> void {
        glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
    }

    auto detail::swap_buffers(GLFWwindow *window) noexcept -> void {
        TGX_ASSERT(window != nullptr);

        glfwSwapBuffers(window);
    }

    auto Window::set_title(const char *title) noexcept -> void {
        TGX_ASSERT(title);

        glfwSetWindowTitle(m_handle, title);
    }

    auto detail::set_vsync(bool enabled) noexcept -> void {
        glfwSwapInterval(enabled ? 1 : 0);
    }

    auto Window::size() const noexcept -> Size {
        int width = 0, height = 0;
        glfwGetWindowSize(m_handle, &width, &height);
        return {width, height};
    }

    auto Window::framebuffer_size() const noexcept -> Size {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_handle, &width, &height);
        return {width, height};
    }

    auto Window::destroy() noexcept -> void {
        if (m_handle) {
            glfwDestroyWindow(m_handle);
            m_handle = nullptr;
        }
    }
}
