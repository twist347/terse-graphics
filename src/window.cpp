#include "tgx/window.h"

#include <utility>

#include <GLFW/glfw3.h>

#include "tgx/assert.h"
#include "tgx/platform.h"

#include "tgx/gl/version.h"

#include "platform_internal.h"

namespace tgx {
    auto Window::create(Platform &platform, const WindowParams &params) noexcept -> Result<Window> {
        TGX_ASSERT_MSG(platform.is_valid(), "creating a window from a moved-from Platform");

        if (params.width <= 0 || params.height <= 0 || params.title == nullptr) {
            return std::unexpected{Error::invalid_argument};
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
            if (glfwGetError(nullptr) == GLFW_VERSION_UNAVAILABLE) {
                return std::unexpected{Error::unsupported};
            }
            return std::unexpected{Error::platform};
        }

        glfwMakeContextCurrent(handle);

        Window window{handle};
        window.set_vsync(params.vsync);
        return window;
    }

    auto Window::gl_loader() const noexcept -> GlLoader {
        TGX_ASSERT(m_handle);

        return glfwGetProcAddress;
    }

    Window::Window(GLFWwindow *handle) noexcept : m_handle{handle} {
        detail::window_opened();
    }

    Window::Window(Window &&other) noexcept
        : m_handle{std::exchange(other.m_handle, nullptr)} {
    }

    auto Window::operator=(Window &&other) noexcept -> Window & {
        if (this != &other) {
            destroy();
            m_handle = std::exchange(other.m_handle, nullptr);
        }
        return *this;
    }

    Window::~Window() {
        destroy();
    }

    auto Window::should_close() const noexcept -> bool {
        TGX_ASSERT(m_handle);

        return glfwWindowShouldClose(m_handle) == GLFW_TRUE;
    }

    auto Window::request_close() noexcept -> void {
        TGX_ASSERT(m_handle);

        glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
    }

    auto Window::swap_buffers() noexcept -> void {
        TGX_ASSERT(m_handle);

        glfwSwapBuffers(m_handle);
    }

    auto Window::set_title(const char *title) noexcept -> void {
        TGX_ASSERT(m_handle);
        TGX_ASSERT(title != nullptr);

        glfwSetWindowTitle(m_handle, title);
    }

    auto Window::set_vsync(bool enabled) noexcept -> void {
        TGX_ASSERT(m_handle);

        glfwSwapInterval(enabled ? 1 : 0);
    }

    auto Window::framebuffer_size() const noexcept -> std::pair<int, int> {
        TGX_ASSERT(m_handle);

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_handle, &width, &height);
        return {width, height};
    }

    auto Window::destroy() noexcept -> void {
        if (m_handle == nullptr) {
            return;
        }

        glfwDestroyWindow(m_handle);
        m_handle = nullptr;

        detail::window_closed();
    }
}
