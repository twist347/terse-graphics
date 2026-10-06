#include "tgx/device.h"

#include "tgx/assert.h"
#include "tgx/render_target.h"
#include "tgx/texture.h"
#include "tgx/window.h"

#include "tgx/gl/draw.h"
#include "tgx/gl/shader.h"
#include "tgx/gl/texture_slot.h"
#include "tgx/gl/version.h"
#include "tgx/gl/vertex_array.h"

#include "batch.h"
#include "context.h"
#include "log_internal.h"
#include "window_internal.h"

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

// Only the 3.3 backend exists: this refuses to build against an unwritten one,
// it is not a switch.
static_assert(
    tgx::gl::version_major == 3 && tgx::gl::version_minor == 3,
    "tgx implements only OpenGL 3.3"
);

namespace {
    using tgx::gl::detail::ComponentKind;

    // This and the three after it serve the asserts in Device::draw alone:
    // with asserts off nothing calls them, and [[maybe_unused]] says that is
    // meant.
    [[maybe_unused, nodiscard]] constexpr auto to_component_kind(
        tgx::gl::VertexFormat format
    ) noexcept -> ComponentKind {
        using enum tgx::gl::VertexFormat;
        switch (format) {
            case uint32: return ComponentKind::uint;
            case sint32: return ComponentKind::sint;
            case float32:
            case float32x2:
            case float32x3:
            case float32x4:
            case unorm8x4: return ComponentKind::floating;
        }
        return ComponentKind::floating;
    }

    [[maybe_unused, nodiscard]] constexpr auto to_str(ComponentKind kind) noexcept -> const char * {
        switch (kind) {
            case ComponentKind::floating: return "float";
            case ComponentKind::sint: return "int";
            case ComponentKind::uint: return "uint";
        }
        return "unknown";
    }

    // GL feeds an input with no attribute a constant (usually 0, 0, 0, 1), and
    // reads integers into a float input or the other way round as garbage;
    // neither is reported. A different component count is fine: GL pads the
    // missing ones with 0, 0, 1.
    [[maybe_unused]] auto check_vertex_inputs(
        const tgx::gl::Shader &shader,
        const tgx::gl::VertexArray &vertices
    ) noexcept -> void {
        const auto attributes = vertices.attributes();
        for (const auto &input: tgx::gl::detail::vertex_inputs(shader)) {
            for (std::uint32_t slot = 0; slot < input.slots; ++slot) {
                const std::uint32_t location = input.location + slot;
                const auto attribute = std::ranges::find(attributes, location, &tgx::gl::VertexAttribute::location);
                TGX_ASSERT_MSG(
                    attribute != attributes.end(),
                    "vertex input '{}' reads location {}, which the vertex array has no attribute for",
                    input.name, location
                );
                TGX_ASSERT_MSG(
                    to_component_kind(attribute->format) == input.kind,
                    "vertex input '{}' at location {} is {}, but its attribute is {}",
                    input.name, location, to_str(input.kind), to_str(to_component_kind(attribute->format))
                );
            }
        }
    }

    // A sampler reads its slot whether or not the draw put a texture there, and
    // gets whatever an earlier draw left, or black. Neither is reported.
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
        // Performance hints are advice, not faults, whatever severity the
        // driver gives them: NVIDIA rates its routine recompile of a shader on
        // first draw as medium.
        if (type == GL_DEBUG_TYPE_PERFORMANCE) {
            return tgx::LogLevel::info;
        }
        switch (severity) {
            case GL_DEBUG_SEVERITY_HIGH: return tgx::LogLevel::error;
            case GL_DEBUG_SEVERITY_MEDIUM: return tgx::LogLevel::warn;
            default: return tgx::LogLevel::info;
        }
    }

    // Plain "void" on purpose: GLAD_API_PTR is a calling-convention macro on
    // some platforms, and it has to sit between the return type and the name.
    void GLAD_API_PTR on_gl_debug(
        GLenum,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei,
        const GLchar *message,
        const void *
    ) noexcept {
        // The message is null-terminated; the reported length is not trusted,
        // as drivers disagree on whether it counts the terminator.
        tgx::detail::log(to_log_level(type, severity), "gl {}: {}", id, message);
    }

    // glGetString hands out unsigned chars; a lost context gives nullptr.
    [[nodiscard]] auto gl_string(GLenum name) noexcept -> std::string_view {
        const auto *str = reinterpret_cast<const char *>(glGetString(name));
        return str ? str : "?";
    }

    auto log_context_info() noexcept -> void {
        tgx::detail::log_info("OpenGL {}", gl_string(GL_VERSION));
        tgx::detail::log_info("  renderer: {}", gl_string(GL_RENDERER));
        tgx::detail::log_info("  vendor:   {}", gl_string(GL_VENDOR));
        tgx::detail::log_info("  glsl:     {}", gl_string(GL_SHADING_LANGUAGE_VERSION));
    }

    // Logs its outcome as the last line of the context info block: with the
    // callback off, silence from the driver means nothing.
    auto install_debug_callback() noexcept -> void {
        // Core only since 4.3. Without KHR_debug (macOS has none) there are
        // simply no driver messages.
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
        tgx::detail::log_info("  debug:    on");
    }
}

namespace tgx {
    auto Device::create(Window &) -> Result<Device> {
        // glad 2 returns the version it loaded, so loading and checking that we
        // got the requested one is the same call.
        const int version = gladLoadGL(detail::gl_loader());
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

        detail::Context &context = detail::context();
        context.reset();

        auto batch = detail::Batch::create();
        if (!batch) {
            context = {};
            return std::unexpected{batch.error()};
        }
        context.batch = std::make_unique<detail::Batch>(std::move(*batch));
        return Device{};
    }

    auto Device::operator=(Device &&other) noexcept -> Device & {
        if (this == &other) {
            return *this;
        }
        release();
        m_owned = std::exchange(other.m_owned, false);
        return *this;
    }

    Device::~Device() {
        release();
    }

    auto Device::clear(const ClearParams &params) noexcept -> void {
        const detail::Target target = detail::target_of(params.target);
        // GL clears what is not there without a word.
        TGX_ASSERT_MSG(
            !params.depth || target.depth,
            "clearing the depth of a render target made without one"
        );
        TGX_ASSERT_MSG(
            !params.stencil || target.framebuffer == 0,
            "clearing the stencil of a render target, which has none"
        );

        detail::context().clear({
            .target = target,
            .color = params.color,
            .depth = params.depth,
            .stencil = params.stencil,
        });
    }

    auto Device::flush() noexcept -> void {
        detail::context().flush();
    }

    auto Device::read() -> Image {
        Image image = detail::context().read({});
        // The window's alpha is whatever blending left in it; what shows is
        // opaque.
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
    }

    auto Device::clock() const noexcept -> const Clock & {
        static const Clock clock;
        return clock;
    }

    auto Device::restart_clock() noexcept -> void {
        detail::context().clock.restart();
    }

    auto Device::draw(const gl::Shader &shader, const gl::VertexArray &vertices) noexcept -> void {
        draw(shader, vertices, {});
    }

    auto Device::draw(
        const gl::Shader &shader,
        const gl::VertexArray &vertices,
        const gl::DrawParams &params
    ) noexcept -> void {
        detail::context().flush();

        TGX_ASSERT_MSG(vertices.vertex_count() > 0, "drawing from a vertex array with no vertex buffer");
        if constexpr (TGX_ENABLE_ASSERTS != 0) {
            check_vertex_inputs(shader, vertices);
            check_textures(shader, params);
        }

        const bool indexed = vertices.has_index_buffer();
        const std::size_t available = indexed ? vertices.index_count() : vertices.vertex_count();
        const std::size_t count = params.count != gl::DrawParams::all
                                      ? params.count
                                      : params.first < available
                                            ? available - params.first
                                            : 0;

        TGX_ASSERT(std::in_range<GLsizei>(count) && std::in_range<GLint>(params.first));

        // GL does not check ranges: reading past a buffer is undefined, and the
        // debug output usually stays silent. Index values themselves are not
        // checked, that would mean reading the buffer back. Both fit a GLint
        // (asserted above), so the sum cannot overflow.
        TGX_ASSERT_MSG(
            params.first + count <= available,
            "drawing {} [{}, {}) from a buffer of {}",
            indexed ? "indices" : "vertices", params.first, params.first + count, available
        );

        const detail::Target target = detail::target_of(params.target);
        // Without a depth buffer every fragment passes the test, which GL
        // does not report.
        TGX_ASSERT_MSG(
            params.state.depth == gl::Depth::none || target.depth,
            "a depth test drawing into a render target made without depth"
        );
        // GL refuses a negative size and draws nothing, without a word unless
        // on a debug context.
        TGX_ASSERT_MSG(
            !params.viewport || (params.viewport->width >= 0 && params.viewport->height >= 0),
            "viewport of size {}x{}",
            params.viewport ? params.viewport->width : 0, params.viewport ? params.viewport->height : 0
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
            .viewport = params.viewport.value_or(detail::surface_of(target).viewport()),
        };
        for (std::uint32_t slot = 0; slot < gl::max_texture_slots; ++slot) {
            if (const Texture *texture = params.textures[slot]) {
                // GL leaves it undefined, and drivers draw garbage.
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
        if (m_owned) {
            // Its gl resources go while GL is still usable, and while the
            // context is still whole for them to unbind from. What it still
            // holds is dropped: the frame it was for is over.
            detail::Context &context = detail::context();
            context.batch.reset();
            context = {};
            m_owned = false;
        }
    }
}
