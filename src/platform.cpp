#include "tgx/platform.h"

#include <utility>

#include <GLFW/glfw3.h>

#include "tgx/assert.h"

#include "log_internal.h"
#include "platform_internal.h"

namespace {
    // Mirrors GLFW's own process-wide state: a second glfwInit is a no-op, but the
    // first glfwTerminate would pull it out from under the other Platform.
    bool s_platform_alive = false;

    // Only one Platform exists at a time, so this count is that platform's. It
    // lives here rather than in the object so that Platform can move while
    // windows are open.
    int s_window_count = 0;

    auto on_glfw_error(int code, const char *desc) noexcept -> void {
        tgx::detail::log_error("glfw {}: {}", code, desc);
    }
}

namespace tgx {
    auto detail::window_opened() noexcept -> void {
        ++s_window_count;
    }

    auto detail::window_closed() noexcept -> void {
        TGX_ASSERT(s_window_count > 0);

        --s_window_count;
    }

    auto Platform::create() noexcept -> Result<Platform> {
        TGX_ASSERT_MSG(!s_platform_alive, "only one Platform may exist at a time");

        glfwSetErrorCallback(on_glfw_error);

        if (glfwInit() != GLFW_TRUE) {
            return std::unexpected{Error::platform};
        }

        s_platform_alive = true;
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

    auto Platform::poll_events() noexcept -> void {
        TGX_ASSERT_MSG(m_owned, "polling events on a moved-from Platform");

        glfwPollEvents();
    }

    auto Platform::shutdown() noexcept -> void {
        if (!m_owned) {
            return;
        }

        TGX_ASSERT_MSG(
            s_window_count == 0,
            "Platform destroyed while {} window(s) are alive",
            s_window_count
        );

        glfwTerminate();
        s_platform_alive = false;
        m_owned = false;
    }
}
