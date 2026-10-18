#include "tgx/core/device.hpp"

#include "tgx/core/assert.hpp"
#include "tgx/core/render_target.hpp"
#include "tgx/core/texture.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/shader.hpp"
#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/version.hpp"
#include "tgx/gl/vertex_array.hpp"

#include "core/batch.hpp"
#include "core/context.hpp"
#include "core/gl_pixels.hpp"
#include "core/log_internal.hpp"
#include "core/window_internal.hpp"
#include "gl/vertex_array_internal.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

// Only the 3.3 backend exists; this is not a version switch.
static_assert(
    tgx::gl::version_major == 3 && tgx::gl::version_minor == 3,
    "tgx implements only OpenGL 3.3"
);

namespace {
    // Asserts each sampler has a texture in its slot. Otherwise it reads what
    // an earlier draw left, or black, and GL reports neither. Unused with
    // asserts off.
    [[maybe_unused]] auto check_textures(
        const tgx::gl::Shader &shader,
        const tgx::gl::DrawParams &params
    ) noexcept -> void {
        for (const auto &sampler: tgx::gl::detail::samplers(shader)) {
            TGX_ASSERT_MSG(
                params.textures[sampler.slot],
                "sampler '{}' reads texture slot {}, which the draw has no texture for",
                sampler.name, sampler.slot
            );
        }
    }

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

    auto log_context_info() noexcept -> void {
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

    // Logged last in the context info: with the callback off, driver
    // silence means nothing.
    auto install_debug_callback() noexcept -> void {
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

namespace tgx {
    auto Device::create(Window &) -> Result<Device> {
        // glad 2 returns the version it loaded.
        const int version = gladLoadGL(detail::gl_loader());
        if (version == 0) {
            return std::unexpected{Error::platform};
        }
        // Before the version check, so a rejected context is still logged.
        log_context_info();
        if (version < GLAD_MAKE_VERSION(gl::version_major, gl::version_minor)) {
            return std::unexpected{Error::unsupported};
        }

        install_debug_callback();

        detail::Context &context = detail::context();
        context.reset();

        auto batch = detail::Batch::create();
        if (!batch) {
            context = {};
            return std::unexpected{batch.error()};
        }
        context.batch = std::make_unique<detail::Batch>(std::move(*batch));

        // One blank frame now: on Wayland with a fractional scale, the buffer
        // gets the size GLFW reports only at a swap, so the first frame would
        // be cut off and flipped with the wrong height. Not present(): it is
        // not an app frame, and the clock does not tick.
        context.clear({.color = colors::black});
        detail::swap_buffers();
        context.frame = {};
        return Device{};
    }

    auto Device::operator=(Device &&other) noexcept -> Device & {
        if (this == &other) {
            return *this;
        }
        release();
        owned_ = std::exchange(other.owned_, false);
        return *this;
    }

    Device::~Device() {
        release();
    }

    auto Device::clear(const ClearParams &params) noexcept -> void {
        const detail::Target target = detail::target_of(params.target);
        // GL silently ignores a clear of a missing buffer.
        TGX_ASSERT_MSG(
            !params.depth || target.depth,
            "clearing the depth of a render target made without one"
        );

        detail::context().clear({
            .target = target,
            .color = params.color,
            .depth = params.depth,
        });
    }

    auto Device::flush() noexcept -> void {
        detail::context().flush();
    }

    auto Device::read() -> Image {
        Image image = detail::context().read({});
        // The window's alpha is whatever blending left, but it shows opaque.
        for (Color &pixel: image.pixels()) {
            pixel.a = 255;
        }
        return image;
    }

    auto Device::present() noexcept -> void {
        detail::Context &context = detail::context();
        context.flush();
        detail::swap_buffers();
        context.clock.tick();
        context.last_frame = std::exchange(context.frame, {});
    }

    auto Device::clock() const noexcept -> const Clock & {
        static constexpr Clock clock{};
        return clock;
    }

    auto Device::restart_clock() noexcept -> void {
        detail::context().clock.restart();
    }

    auto Device::stats() const noexcept -> FrameStats {
        return detail::context().last_frame;
    }

    auto Device::draw(const gl::Shader &shader, const gl::VertexArray &vertices) noexcept -> void {
        draw(shader, vertices, {});
    }

    auto Device::draw(
        const gl::Shader &shader,
        const gl::VertexArray &vertices,
        const gl::DrawParams &params
    ) noexcept -> void {
        TGX_ASSERT_MSG(vertices.vertex_count() > 0, "drawing from a vertex array with no vertex buffer");
        if constexpr (TGX_ENABLE_ASSERTS != 0) {
            gl::detail::check_vertex_inputs(shader, vertices);
            check_textures(shader, params);
        }

        const bool indexed = vertices.has_index_buffer();
        const std::size_t available = indexed ? vertices.index_count() : vertices.vertex_count();
        const std::size_t count = params.count != gl::DrawParams::all
                                      ? params.count
                                      : available - std::min(params.first, available);

        TGX_ASSERT(std::in_range<GLsizei>(count) && std::in_range<GLint>(params.first));

        // GL does not check ranges: reading past a buffer is undefined and
        // usually silent. Index values are not checked; that would need a read
        // back. Both values fit a GLint, so the sum cannot overflow.
        TGX_ASSERT_MSG(
            params.first + count <= available,
            "drawing {} [{}, {}) from a buffer of {}",
            indexed ? "indices" : "vertices", params.first, params.first + count, available
        );

        const detail::Target target = detail::target_of(params.target);
        // Without a depth buffer every fragment passes, silently.
        TGX_ASSERT_MSG(
            params.state.depth == gl::Depth::none || target.depth,
            "a depth test drawing into a render target made without depth"
        );
        const gl::Viewport viewport = params.viewport.value_or(detail::surface_of(target).viewport());
        // GL draws nothing, and reports it only on a debug context.
        TGX_ASSERT_MSG(
            viewport.width >= 0 && viewport.height >= 0,
            "viewport of size {}x{}",
            viewport.width, viewport.height
        );
        detail::DrawCall call{
            .target = target,
            .program = shader.id(),
            .vertex_array = vertices.id(),
            .index_type = indexed ? std::optional{vertices.index_type()} : std::nullopt,
            .first = params.first,
            .count = count,
            .primitive = params.primitive,
            .state = params.state,
            .viewport = viewport,
        };
        for (std::uint32_t slot = 0; slot < gl::max_texture_slots; ++slot) {
            if (const Texture *texture = params.textures[slot]) {
                // Undefined in GL; drivers draw garbage.
                TGX_ASSERT_MSG(
                    texture->id() != target.texture,
                    "slot {} reads the texture of the render target the draw writes",
                    slot
                );
                call.textures[slot] = texture->id();
            }
        }
        detail::context().draw(call);
    }

    auto Device::release() noexcept -> void {
        if (owned_) {
            // The batch goes while GL and the Context are still whole. Its
            // shapes are dropped: their frame is over.
            detail::Context &context = detail::context();
            context.batch.reset();
            context = {};
            owned_ = false;
        }
    }
}
