#include "tgx/device.h"

#include "tgx/assert.h"
#include "tgx/window.h"

#include "tgx/gl/handle.h"
#include "tgx/gl/shader.h"
#include "tgx/gl/version.h"
#include "tgx/gl/vertex_array.h"

#include "device_internal.h"
#include "log_internal.h"
#include "window_internal.h"

#include <glad/gl.h>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

static_assert(std::is_same_v<GLuint, tgx::gl::GlId>);

// Only the 3.3 backend exists: this refuses to build against an unwritten one,
// it is not a switch.
static_assert(
    tgx::gl::version_major == 3 && tgx::gl::version_minor == 3,
    "tgx implements only OpenGL 3.3"
);

namespace {
    using tgx::gl::detail::ComponentKind;

    // Whether a Device exists, i.e. GL may be called. Main thread only, like
    // the rest of the Device.
    bool s_device_alive = false;

    // The program glUseProgram last made current; 0 for none.
    GLuint s_current_program = 0;

    auto apply_clear_color(tgx::Color color) noexcept -> void {
        const tgx::Vec4 unit = tgx::to_vec4(color);
        glClearColor(unit.x, unit.y, unit.z, unit.w);
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

    [[nodiscard]] constexpr auto to_gl(tgx::gl::IndexType type) noexcept -> GLenum {
        using enum tgx::gl::IndexType;
        switch (type) {
            case uint16: return GL_UNSIGNED_SHORT;
            case uint32: return GL_UNSIGNED_INT;
        }
        return GL_UNSIGNED_SHORT;
    }

    [[nodiscard]] constexpr auto to_component_kind(tgx::gl::VertexFormat format) noexcept -> ComponentKind {
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

    [[nodiscard]] constexpr auto to_str(ComponentKind kind) noexcept -> const char * {
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
    auto check_vertex_inputs(
        const tgx::gl::Shader &shader,
        const tgx::gl::VertexArray &vertices
    ) noexcept -> void {
        const auto attributes = vertices.attributes();
        for (const auto &input : tgx::gl::detail::vertex_inputs(shader)) {
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
        return str != nullptr ? str : "?";
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
    auto gl::detail::context_alive() noexcept -> bool {
        return s_device_alive;
    }

    auto detail::use_program(gl::GlId program) noexcept -> void {
        if (program != s_current_program) {
            glUseProgram(program);
            s_current_program = program;
        }
    }

    auto detail::forget_program(gl::GlId program) noexcept -> void {
        if (program == s_current_program) {
            s_current_program = 0;
        }
    }

    auto Device::create(Window &window) noexcept -> Result<Device> {
        TGX_ASSERT_MSG(!s_device_alive, "only one Device may exist at a time");

        // glad 2 returns the version it loaded, so loading and checking that we
        // got the requested one is the same call.
        const int version = gladLoadGL(detail::gl_loader(window));
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
        device.set_viewport(window.framebuffer_size());
        apply_clear_color(device.m_clear_color);
        return device;
    }

    Device::Device() noexcept : m_owned{true} {
        s_device_alive = true;
        s_current_program = 0;
    }

    Device::Device(Device &&other) noexcept
        : m_clear_color{other.m_clear_color},
          m_viewport{other.m_viewport},
          m_owned{std::exchange(other.m_owned, false)} {
    }

    auto Device::operator=(Device &&other) noexcept -> Device & {
        if (this == &other) {
            return *this;
        }
        release();
        m_clear_color = other.m_clear_color;
        m_viewport = other.m_viewport;
        m_owned = std::exchange(other.m_owned, false);
        return *this;
    }

    Device::~Device() {
        release();
    }

    auto Device::release() noexcept -> void {
        if (m_owned) {
            s_device_alive = false;
            s_current_program = 0;
            m_owned = false;
        }
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
        TGX_ASSERT_MSG(vertices.vertex_count() > 0, "drawing from a vertex array with no vertex buffer");
        if constexpr (TGX_ENABLE_ASSERTS != 0) {
            check_vertex_inputs(shader, vertices);
        }

        const bool indexed = vertices.has_index_buffer();
        const std::size_t available = indexed ? vertices.index_count() : vertices.vertex_count();
        const std::size_t count = params.count != DrawParams::all ? params.count
            : params.first < available ? available - params.first
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

        if (count == 0) {
            return;
        }

        detail::use_program(shader.id());
        glBindVertexArray(vertices.id());

        const GLenum mode = to_gl(params.primitive);
        const auto gl_count = static_cast<GLsizei>(count);

        if (!indexed) {
            glDrawArrays(mode, static_cast<GLint>(params.first), gl_count);
            return;
        }

        const gl::IndexType index_type = vertices.index_type();
        const std::size_t index_size = index_type == gl::IndexType::uint32 ? 4 : 2;

        // GL takes the start of an indexed draw as a byte offset into the index
        // buffer, passed where a pointer used to go.
        const auto *start = reinterpret_cast<const void *>(params.first * index_size);
        glDrawElements(mode, gl_count, to_gl(index_type), start);
    }
}
