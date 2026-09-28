#include "tgx/device.h"

#include "tgx/gl/version.h"
#include "tgx/window.h"

#include <cstdint>
#include <cstdio>

#include <glad/gl.h>

// Only the 4.5 backend exists: this refuses to build against an unwritten one,
// it is not a switch.
static_assert(
    tgx::gl::version_major == 4 && tgx::gl::version_minor == 5,
    "tgx implements only OpenGL 4.5"
);

namespace {
    [[nodiscard]] constexpr auto to_unit(std::uint8_t channel) noexcept -> float {
        return static_cast<float>(channel) / 255.0F;
    }

    void apply_clear_color(const tgx::Color &color) noexcept {
        glClearColor(to_unit(color.r), to_unit(color.g), to_unit(color.b), to_unit(color.a));
    }

    void GLAD_API_PTR on_gl_debug(
        GLenum,
        GLenum,
        GLuint id,
        GLenum,
        GLsizei,
        const GLchar *message,
        const void *
    ) noexcept {
        std::fprintf(stderr, "[tgx] gl %u: %s\n", id, message);
    }

    void install_debug_callback() noexcept {
        GLint flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if ((flags & GL_CONTEXT_FLAG_DEBUG_BIT) == 0) {
            return;
        }

        glEnable(GL_DEBUG_OUTPUT);
        // Report from inside the offending call, so a breakpoint in the
        // callback shows the caller.
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(on_gl_debug, nullptr);
        // Notifications are driver chatter (buffer placement and the like).
        glDebugMessageControl(
            GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION,
            0,
            nullptr,
            GL_FALSE
        );
    }
}

namespace tgx {
    auto Device::create(Window &window) noexcept -> Result<Device> {
        // glad 2 returns the version it loaded, so loading and checking that we
        // got the requested one is the same call.
        const int version = gladLoadGL(window.gl_loader());
        if (version == 0) {
            return std::unexpected{Error::platform};
        }
        if (version < GLAD_MAKE_VERSION(gl::version_major, gl::version_minor)) {
            return std::unexpected{Error::unsupported};
        }

        install_debug_callback();

        Device device;
        const auto [width, height] = window.framebuffer_size();
        device.set_viewport(0, 0, width, height);
        apply_clear_color(device.m_clear_color);
        return device;
    }

    void Device::set_clear_color(const Color &color) noexcept {
        if (color == m_clear_color) {
            return;
        }

        m_clear_color = color;
        apply_clear_color(color);
    }

    void Device::clear(ClearMask mask) noexcept {
        GLbitfield bits = 0;

        if (has(mask, ClearMask::color)) {
            bits |= GL_COLOR_BUFFER_BIT;
        }
        if (has(mask, ClearMask::depth)) {
            bits |= GL_DEPTH_BUFFER_BIT;
        }
        if (has(mask, ClearMask::stencil)) {
            bits |= GL_STENCIL_BUFFER_BIT;
        }

        if (bits != 0) {
            glClear(bits);
        }
    }

    void Device::set_viewport(int x, int y, int width, int height) noexcept {
        const std::array next{x, y, width, height};
        if (next == m_viewport) {
            return;
        }

        m_viewport = next;
        glViewport(x, y, width, height);
    }
}
