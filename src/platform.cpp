#include "tgx/platform.h"

#include <cstdio>

#include <GLFW/glfw3.h>

#include "tgx/assert.h"

namespace {
    // Mirrors GLFW's own process-wide state: a second glfwInit is a no-op, but the
    // first glfwTerminate would pull it out from under the other Platform.
    bool s_platform_alive = false;

    void on_glfw_error(int code, const char *desc) noexcept {
        std::fprintf(stderr, "[tgx] glfw error %d: %s\n", code, desc);
    }
}

namespace tgx {
    auto Platform::create() noexcept -> Result<Platform> {
        TGX_ASSERT_MSG(!s_platform_alive, "only one Platform may exist at a time");

        glfwSetErrorCallback(on_glfw_error);

        if (glfwInit() != GLFW_TRUE) {
            return std::unexpected{Error::platform};
        }

        s_platform_alive = true;

        // Platform is pinned, so it can't be moved into the result; transform
        // constructs it in place from the prvalue.
        return Result<void>{}.transform([] { return Platform{}; });
    }

    Platform::~Platform() {
        TGX_ASSERT_MSG(
            m_window_count == 0,
            "Platform destroyed while {} window(s) are alive",
            m_window_count
        );

        glfwTerminate();
        s_platform_alive = false;
    }

    void Platform::poll_events() noexcept {
        glfwPollEvents();
    }
}
