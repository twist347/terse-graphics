#include "core/gl_debug.hpp"

#include "tgx/core/log.hpp"

#include "core/gl_pixels.hpp"
#include "core/log_internal.hpp"

#include <glad/gl.h>

#include <string_view>

namespace {
    [[nodiscard]] constexpr auto to_log_level(GLenum type, GLenum severity) noexcept -> tgx::LogLevel {
        // Performance hints are advice, whatever their severity: NVIDIA rates
        // its routine shader recompile on first draw as medium.
        if (type == GL_DEBUG_TYPE_PERFORMANCE) {
            return tgx::LogLevel::info;
        }
        switch (severity) {
            case GL_DEBUG_SEVERITY_HIGH: return tgx::LogLevel::error;
            case GL_DEBUG_SEVERITY_MEDIUM: return tgx::LogLevel::warn;
            default: return tgx::LogLevel::info;
        }
    }

    // Plain "void": GLAD_API_PTR, a calling-convention macro, must go between
    // the return type and the name.
    void GLAD_API_PTR on_gl_debug(
        GLenum,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei,
        const GLchar *message,
        const void *
    ) noexcept {
        // Drivers disagree on whether the length counts the terminator, so
        // the null-terminated message is used as is.
        tgx::detail::log(to_log_level(type, severity), "gl {}: {}", id, message);
    }

    // glGetString returns unsigned chars, and nullptr on a lost context.
    [[nodiscard]] auto gl_string(GLenum name) noexcept -> std::string_view {
        const auto *str = reinterpret_cast<const char *>(glGetString(name));
        return str ? str : "?";
    }
}

namespace tgx {
    auto detail::log_context_info() noexcept -> void {
        tgx::detail::log_info("OpenGL {}", gl_string(GL_VERSION));
        tgx::detail::log_info("  renderer: {}", gl_string(GL_RENDERER));
        tgx::detail::log_info("  vendor:   {}", gl_string(GL_VENDOR));
        tgx::detail::log_info("  glsl:     {}", gl_string(GL_SHADING_LANGUAGE_VERSION));
        const int max_size = tgx::detail::max_texture_size();
        tgx::detail::log_info("  textures: up to {}x{}", max_size, max_size);
        GLint samples = 0;
        glGetIntegerv(GL_SAMPLES, &samples);
        if (samples > 0) {
            tgx::detail::log_info("  msaa:     {} samples", samples);
        } else {
            tgx::detail::log_info("  msaa:     off");
        }
    }

    auto detail::install_debug_callback() noexcept -> void {
        // Core only since 4.3. macOS has no KHR_debug.
        if (GLAD_GL_KHR_debug == 0) {
            tgx::detail::log_info("  debug:    off (no KHR_debug)");
            return;
        }

        GLint flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if ((flags & GL_CONTEXT_FLAG_DEBUG_BIT) == 0) {
            tgx::detail::log_info("  debug:    off (not a debug context)");
            return;
        }

        glEnable(GL_DEBUG_OUTPUT);
        // So a breakpoint in the callback shows the failing call.
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
        tgx::detail::log_info("  debug:    on");
    }
}
