#include "tgx/platform.h"

#include "log_internal.h"

#include <GLFW/glfw3.h>

#include <utility>

namespace {
    auto on_glfw_error(int code, const char *desc) noexcept -> void {
        tgx::detail::log_error("glfw {}: {}", code, desc);
    }
}

namespace tgx {
    auto Platform::create() noexcept -> Result<Platform> {
        glfwSetErrorCallback(on_glfw_error);

        if (glfwInit() != GLFW_TRUE) {
            return std::unexpected{Error::platform};
        }
        return Platform{};
    }

    auto Platform::operator=(Platform &&other) noexcept -> Platform & {
        if (this == &other) {
            return *this;
        }
        shutdown();
        m_owned = std::exchange(other.m_owned, false);
        return *this;
    }

    Platform::~Platform() {
        shutdown();
    }

    auto Platform::poll_events() noexcept -> void {
        glfwPollEvents();
    }

    auto Platform::shutdown() noexcept -> void {
        if (m_owned) {
            glfwTerminate();
            m_owned = false;
        }
    }
}
