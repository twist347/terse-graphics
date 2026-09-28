#include "tgx/window.h"

#include <utility>

#include <GLFW/glfw3.h>

#include "tgx/assert.h"
#include "tgx/gl/version.h"
#include "tgx/platform.h"

namespace tgx {
    auto Window::create(Platform &platform, const WindowParams &params) noexcept -> Result<Window> {
        if (params.width <= 0 || params.height <= 0 || params.title == nullptr) {
            return std::unexpected{Error::invalid_argument};
        }

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl::version_major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl::version_minor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
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

        Window window{platform, handle};
        window.set_vsync(params.vsync);
        return window;
    }

    auto Window::gl_loader() const noexcept -> GlLoader {
        TGX_ASSERT(m_handle);

        return glfwGetProcAddress;
    }

    Window::Window(Platform &platform, GLFWwindow *handle) noexcept : m_platform{&platform}, m_handle{handle} {
        ++m_platform->m_window_count;
    }

    Window::Window(Window &&other) noexcept
        : m_platform{std::exchange(other.m_platform, nullptr)},
          m_handle{std::exchange(other.m_handle, nullptr)} {
    }

    auto Window::operator=(Window &&other) noexcept -> Window & {
        if (this != &other) {
            destroy();
            m_platform = std::exchange(other.m_platform, nullptr);
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

    void Window::request_close() noexcept {
        TGX_ASSERT(m_handle);

        glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
    }

    void Window::swap_buffers() noexcept {
        TGX_ASSERT(m_handle);

        glfwSwapBuffers(m_handle);
    }

    void Window::set_title(const char *title) noexcept {
        TGX_ASSERT(m_handle);
        TGX_ASSERT(title != nullptr);

        glfwSetWindowTitle(m_handle, title);
    }

    void Window::set_vsync(bool enabled) noexcept {
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

    void Window::destroy() noexcept {
        if (m_handle == nullptr) {
            return;
        }

        glfwDestroyWindow(m_handle);
        m_handle = nullptr;

        --m_platform->m_window_count;
        m_platform = nullptr;
    }
}
