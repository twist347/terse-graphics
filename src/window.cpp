#include "tgx/window.h"

#include <string>
#include <utility>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "tgx/assert.h"
#include "tgx/platform.h"

namespace tgx {
    auto Window::create(Platform &platform, const WindowParams &params) noexcept -> Result<Window> {
        TGX_ASSERT_MSG(platform.is_valid(), "creating a window from a moved-from Platform");

        if (params.width <= 0 || params.height <= 0 || params.title == nullptr) {
            return std::unexpected{Error::invalid_argument};
        }

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, params.debug_context ? GLFW_TRUE : GLFW_FALSE);

        GLFWwindow *handle = glfwCreateWindow(
            params.width, params.height, params.title, nullptr, nullptr
        );
        if (handle == nullptr) {
            return std::unexpected{Error::platform};
        }

        glfwMakeContextCurrent(handle);

        // glad 2 returns the version it loaded, so asking for 4.5 and checking we
        // got it is the same call.
        const int version = gladLoadGL(glfwGetProcAddress);
        if (version == 0) {
            glfwDestroyWindow(handle);
            return std::unexpected{Error::platform};
        }
        if (GLAD_VERSION_MAJOR(version) < 4
            || (GLAD_VERSION_MAJOR(version) == 4 && GLAD_VERSION_MINOR(version) < 5)) {
            glfwDestroyWindow(handle);
            return std::unexpected{Error::unsupported};
        }

        if (params.debug_context) {
            // detail::install_gl_debug_callback();
        }

        Window window{handle};
        window.set_vsync(params.vsync);
        return window;
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

    void Window::request_close() noexcept {
        TGX_ASSERT(m_handle);

        glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
    }

    void Window::swap_buffers() noexcept {
        TGX_ASSERT(m_handle);

        glfwSwapBuffers(m_handle);
    }

    void Window::set_title(std::string_view title) noexcept {
        TGX_ASSERT(m_handle);

        const std::string owned{title};
        glfwSetWindowTitle(m_handle, owned.c_str());
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
    }
}
