#include "tgx/platform.h"

#include <cstdio>
#include <utility>

#include <GLFW/glfw3.h>

#include "tgx/assert.h"

namespace {
    void on_glfw_error(int code, const char *description) {
        std::fprintf(stderr, "[tgx] glfw error %d: %s\n", code, description);
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

    Platform::Platform(Platform &&other) noexcept
        : m_owned{std::exchange(other.m_owned, false)} {
    }

    auto Platform::operator=(Platform &&other) noexcept -> Platform & {
        if (this != &other) {
            shutdown();
            m_owned = std::exchange(other.m_owned, false);
        }
        return *this;
    }

    Platform::~Platform() {
        shutdown();
    }

    void Platform::poll_events() noexcept {
        TGX_ASSERT(m_owned);

        glfwPollEvents();
    }

    void Platform::shutdown() noexcept {
        if (!m_owned) {
            return;
        }

        glfwTerminate();
        m_owned = false;
    }
}
