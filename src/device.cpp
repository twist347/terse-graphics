#include "tgx/device.h"

#include "tgx/assert.h"
#include "tgx/window.h"

#include "tgx/gl/handle.h"
#include "tgx/gl/shader.h"
#include "tgx/gl/version.h"
#include "tgx/gl/vertex_array.h"

#include "log_internal.h"

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include <glad/gl.h>

static_assert(std::is_same_v<GLuint, tgx::gl::GlId>);

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

    auto apply_clear_color(tgx::Color color) noexcept -> void {
        glClearColor(to_unit(color.r), to_unit(color.g), to_unit(color.b), to_unit(color.a));
    }

    [[nodiscard]] constexpr auto to_gl(tgx::Primitive primitive) noexcept -> GLenum {
        using enum tgx::Primitive;
        switch (primitive) {
            case triangles: return GL_TRIANGLES;
            case triangle_strip: return GL_TRIANGLE_STRIP;
            case lines: return GL_LINES;
            case line_strip: return GL_LINE_STRIP;
            case points: return GL_POINTS;
        }
        return GL_TRIANGLES;
    }

    // Plain "void" on purpose: GLAD_API_PTR is a calling-convention macro on
    // some platforms, and it has to sit between the return type and the name.
    [[nodiscard]] constexpr auto to_log_level(GLenum severity) noexcept -> tgx::LogLevel {
        switch (severity) {
            case GL_DEBUG_SEVERITY_HIGH: return tgx::LogLevel::error;
            case GL_DEBUG_SEVERITY_MEDIUM: return tgx::LogLevel::warn;
            default: return tgx::LogLevel::info;
        }
    }

    void GLAD_API_PTR on_gl_debug(
        GLenum,
        GLenum,
        GLuint id,
        GLenum severity,
        GLsizei,
        const GLchar *message,
        const void *
    ) noexcept {
        // The message is null-terminated; the reported length is not trusted,
        // as drivers disagree on whether it counts the terminator.
        tgx::detail::log(to_log_level(severity), "gl {}: {}", id, message);
    }

    // glGetString hands out unsigned chars; a lost context gives nullptr.
    [[nodiscard]] auto gl_string(GLenum name) noexcept -> std::string_view {
        const auto *str = reinterpret_cast<const char *>(glGetString(name));
        return str != nullptr ? str : "?";
    }

    auto log_context_info() noexcept -> void {
        tgx::detail::log_info("OpenGL {}", gl_string(GL_VERSION));
        tgx::detail::log_info("  renderer: {}", gl_string(GL_RENDERER));
        tgx::detail::log_info("  vendor:   {}", gl_string(GL_VENDOR));
        tgx::detail::log_info("  glsl:     {}", gl_string(GL_SHADING_LANGUAGE_VERSION));
    }

    auto install_debug_callback() noexcept -> void {
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
        // Before the version check, so a rejected context still shows what the
        // driver actually gave.
        log_context_info();
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

    auto Device::set_clear_color(Color color) noexcept -> void {
        if (color == m_clear_color) {
            return;
        }

        m_clear_color = color;
        apply_clear_color(color);
    }

    auto Device::clear(ClearMask mask) noexcept -> void {
        GLbitfield bits = 0;

        if (any_of(mask, ClearMask::color)) {
            bits |= GL_COLOR_BUFFER_BIT;
        }
        if (any_of(mask, ClearMask::depth)) {
            bits |= GL_DEPTH_BUFFER_BIT;
        }
        if (any_of(mask, ClearMask::stencil)) {
            bits |= GL_STENCIL_BUFFER_BIT;
        }

        if (bits != 0) {
            glClear(bits);
        }
    }

    auto Device::set_viewport(int x, int y, int width, int height) noexcept -> void {
        const std::array next{x, y, width, height};
        if (next == m_viewport) {
            return;
        }

        m_viewport = next;
        glViewport(x, y, width, height);
    }

    auto Device::draw(
        const gl::Shader &shader,
        const gl::VertexArray &vertices,
        const DrawParams &params
    ) noexcept -> void {
        TGX_ASSERT(std::in_range<GLsizei>(params.count) && std::in_range<GLint>(params.first));

        if (params.count == 0) {
            return;
        }

        glUseProgram(shader.id());
        glBindVertexArray(vertices.id());

        const GLenum mode = to_gl(params.primitive);
        const auto count = static_cast<GLsizei>(params.count);

        if (!vertices.has_index_buffer()) {
            glDrawArrays(mode, static_cast<GLint>(params.first), count);
            return;
        }

        const bool wide = vertices.index_type() == gl::IndexType::uint32;
        const GLenum type = wide ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
        const std::size_t index_size = wide ? 4 : 2;

        // GL takes the start of an indexed draw as a byte offset into the index
        // buffer, passed where a pointer used to go.
        const auto *start = reinterpret_cast<const void *>(params.first * index_size);
        glDrawElements(mode, count, type, start);
    }
}
