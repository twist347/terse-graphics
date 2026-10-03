#include "tgx/gl/device.h"

#include "tgx/assert.h"
#include "tgx/texture.h"
#include "tgx/window.h"

#include "tgx/gl/handle.h"
#include "tgx/gl/shader.h"
#include "tgx/gl/texture_slot.h"
#include "tgx/gl/version.h"
#include "tgx/gl/vertex_array.h"

#include "batch.h"
#include "device_internal.h"
#include "log_internal.h"
#include "window_internal.h"

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
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

    // Where that Device is now; follows it through moves.
    tgx::gl::Device *s_device = nullptr;

    // The program glUseProgram last made current; 0 for none.
    GLuint s_current_program = 0;

    // The texture bound in each slot, 0 for none, and the slot glActiveTexture
    // last selected.
    std::array<GLuint, tgx::gl::max_texture_slots> s_bound_textures{};
    std::uint32_t s_active_slot = 0;

    // Forgets every binding, for a Device starting on or leaving a context.
    auto reset_bindings() noexcept -> void {
        s_current_program = 0;
        s_bound_textures = {};
        s_active_slot = 0;
    }

    auto apply_clear_color(tgx::Color color) noexcept -> void {
        const tgx::Vec4 unit = tgx::to_vec4(color);
        glClearColor(unit.x, unit.y, unit.z, unit.w);
    }

    auto apply_clear_depth(float depth) noexcept -> void {
        glClearDepth(static_cast<GLdouble>(depth));
    }

    [[nodiscard]] constexpr auto to_gl(tgx::gl::Primitive primitive) noexcept -> GLenum {
        using enum tgx::gl::Primitive;
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

    struct GlBlend {
        GLenum src_rgb;
        GLenum dst_rgb;
        GLenum src_alpha;
        GLenum dst_alpha;
    };

    // The alpha channel is kept as coverage: it builds up the way paint would,
    // which is what a later draw of this framebuffer as a texture expects.
    [[nodiscard]] constexpr auto to_gl(tgx::Blend blend) noexcept -> GlBlend {
        using enum tgx::Blend;
        switch (blend) {
            case none: return {GL_ONE, GL_ZERO, GL_ONE, GL_ZERO};
            case alpha: return {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA};
            case premultiplied: return {GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA};
            case additive: return {GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE};
            case multiply: return {GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE};
        }
        return {GL_ONE, GL_ZERO, GL_ONE, GL_ZERO};
    }

    [[nodiscard]] constexpr auto to_gl(tgx::gl::Depth depth) noexcept -> GLenum {
        using enum tgx::gl::Depth;
        switch (depth) {
            case none: return GL_ALWAYS;
            case less: return GL_LESS;
            case less_equal: return GL_LEQUAL;
        }
        return GL_ALWAYS;
    }

    [[nodiscard]] constexpr auto to_gl(tgx::gl::Cull cull) noexcept -> GLenum {
        using enum tgx::gl::Cull;
        switch (cull) {
            // Never reaches GL: culling is switched off instead.
            case none:
            case back: return GL_BACK;
            case front: return GL_FRONT;
        }
        return GL_BACK;
    }

    [[nodiscard]] constexpr auto to_gl(tgx::gl::Fill fill) noexcept -> GLenum {
        using enum tgx::gl::Fill;
        switch (fill) {
            case solid: return GL_FILL;
            case wireframe: return GL_LINE;
        }
        return GL_FILL;
    }

    auto set_enabled(GLenum capability, bool enabled) noexcept -> void {
        if (enabled) {
            glEnable(capability);
        } else {
            glDisable(capability);
        }
    }

    // Puts GL where a default RenderState says it is. A context can outlive a
    // Device and keep what the last one set.
    auto reset_state() noexcept -> void {
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glActiveTexture(GL_TEXTURE0);
    }

    // Brings GL from the state current describes to next, touching only what
    // differs.
    auto apply_state(tgx::gl::RenderState &current, const tgx::gl::RenderState &next) noexcept -> void {
        if (next.blend != current.blend) {
            if ((next.blend == tgx::Blend::none) != (current.blend == tgx::Blend::none)) {
                set_enabled(GL_BLEND, next.blend != tgx::Blend::none);
            }
            if (next.blend != tgx::Blend::none) {
                const GlBlend factors = to_gl(next.blend);
                glBlendFuncSeparate(factors.src_rgb, factors.dst_rgb, factors.src_alpha, factors.dst_alpha);
            }
        }

        if (next.depth != current.depth) {
            if ((next.depth == tgx::gl::Depth::none) != (current.depth == tgx::gl::Depth::none)) {
                set_enabled(GL_DEPTH_TEST, next.depth != tgx::gl::Depth::none);
            }
            if (next.depth != tgx::gl::Depth::none) {
                glDepthFunc(to_gl(next.depth));
            }
        }

        if (next.depth_write != current.depth_write) {
            glDepthMask(next.depth_write ? GL_TRUE : GL_FALSE);
        }

        if (next.cull != current.cull) {
            if ((next.cull == tgx::gl::Cull::none) != (current.cull == tgx::gl::Cull::none)) {
                set_enabled(GL_CULL_FACE, next.cull != tgx::gl::Cull::none);
            }
            if (next.cull != tgx::gl::Cull::none) {
                glCullFace(to_gl(next.cull));
            }
        }

        if (next.fill != current.fill) {
            glPolygonMode(GL_FRONT_AND_BACK, to_gl(next.fill));
        }

        current = next;
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

    // A sampler reads its slot whether or not the draw put a texture there, and
    // gets whatever an earlier draw left, or black. Neither is reported.
    auto check_textures(
        const tgx::gl::Shader &shader,
        const tgx::gl::DrawParams &params
    ) noexcept -> void {
        for (const auto &sampler : tgx::gl::detail::samplers(shader)) {
            TGX_ASSERT_MSG(
                params.textures[sampler.slot] != nullptr,
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

    auto detail::bind_texture(std::uint32_t slot, gl::GlId texture) noexcept -> void {
        TGX_ASSERT(slot < gl::max_texture_slots);

        if (slot != s_active_slot) {
            glActiveTexture(GL_TEXTURE0 + slot);
            s_active_slot = slot;
        }
        if (texture != s_bound_textures[slot]) {
            glBindTexture(GL_TEXTURE_2D, texture);
            s_bound_textures[slot] = texture;
        }
    }

    auto detail::forget_texture(gl::GlId texture) noexcept -> void {
        std::ranges::replace(s_bound_textures, texture, GLuint{0});
    }

    auto detail::device() noexcept -> gl::Device & {
        TGX_ASSERT_MSG(s_device != nullptr, "no Device");

        return *s_device;
    }

    auto detail::flush_texture_use(gl::GlId texture) noexcept -> void {
        // No Device, or one still making or already dropping its batch (whose
        // own white texture comes through here): nothing collected to draw.
        if (s_device == nullptr) {
            return;
        }
        gl::detail::Batch *const batch = gl::detail::DeviceAccess::batch_if_any(*s_device);
        if (batch != nullptr && batch->uses_texture(texture)) {
            batch->flush(*s_device);
        }
    }

    auto detail::flush_shader_use(gl::GlId program) noexcept -> void {
        if (s_device == nullptr) {
            return;
        }
        gl::detail::Batch *const batch = gl::detail::DeviceAccess::batch_if_any(*s_device);
        if (batch != nullptr && batch->uses_shader(program)) {
            batch->flush(*s_device);
        }
    }

    auto gl::detail::DeviceAccess::batch(Device &device) noexcept -> Batch & {
        TGX_ASSERT(device.m_batch != nullptr);

        return *device.m_batch;
    }

    auto gl::detail::DeviceAccess::batch_if_any(Device &device) noexcept -> Batch * {
        return device.m_batch.get();
    }

    auto gl::Device::create(Window &window) noexcept -> Result<Device> {
        TGX_ASSERT_MSG(!s_device_alive, "only one Device may exist at a time");

        // glad 2 returns the version it loaded, so loading and checking that we
        // got the requested one is the same call.
        const int version = gladLoadGL(tgx::detail::gl_loader(window));
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
        device.m_window = window.native_handle();
        reset_state();
        device.set_viewport(window.framebuffer_size());
        // Set rather than assumed: a context can outlive a Device and keep
        // what the last one set.
        apply_clear_color(device.m_clear_color);
        apply_clear_depth(device.m_clear_depth);
        glClearStencil(device.m_clear_stencil);

        // Made of gl resources, so only once the Device marks GL usable.
        auto batch = gl::detail::Batch::create();
        if (!batch) {
            return std::unexpected{batch.error()};
        }
        device.m_batch = std::make_unique<gl::detail::Batch>(std::move(*batch));
        return device;
    }

    gl::Device::Device() noexcept : m_owned{true} {
        s_device_alive = true;
        s_device = this;
        reset_bindings();
    }

    gl::Device::Device(Device &&other) noexcept
        : m_clear_color{other.m_clear_color},
          m_clear_depth{other.m_clear_depth},
          m_clear_stencil{other.m_clear_stencil},
          m_viewport{other.m_viewport},
          m_state{other.m_state},
          m_window{other.m_window},
          m_batch{std::move(other.m_batch)},
          m_owned{std::exchange(other.m_owned, false)} {
        if (m_owned) {
            s_device = this;
        }
    }

    auto gl::Device::operator=(Device &&other) noexcept -> Device & {
        if (this == &other) {
            return *this;
        }
        release();
        m_clear_color = other.m_clear_color;
        m_clear_depth = other.m_clear_depth;
        m_clear_stencil = other.m_clear_stencil;
        m_viewport = other.m_viewport;
        m_state = other.m_state;
        m_window = other.m_window;
        m_batch = std::move(other.m_batch);
        m_owned = std::exchange(other.m_owned, false);
        if (m_owned) {
            s_device = this;
        }
        return *this;
    }

    gl::Device::~Device() {
        release();
    }

    auto gl::Device::release() noexcept -> void {
        if (m_owned) {
            // Its gl resources go while GL is still usable. What it still
            // holds is dropped: the frame it was for is over.
            m_batch.reset();
            s_device_alive = false;
            s_device = nullptr;
            reset_bindings();
            m_owned = false;
        }
    }

    auto gl::Device::flush() noexcept -> void {
        if (m_batch != nullptr) {
            m_batch->flush(*this);
        }
    }

    auto gl::Device::present() noexcept -> void {
        flush();
        tgx::detail::swap_buffers(m_window);
    }

    auto gl::Device::set_vsync(bool enabled) noexcept -> void {
        tgx::detail::set_vsync(enabled);
    }

    auto gl::Device::clear(const ClearParams &params) noexcept -> void {
        flush();

        GLbitfield bits = 0;

        if (params.color) {
            if (*params.color != m_clear_color) {
                m_clear_color = *params.color;
                apply_clear_color(m_clear_color);
            }
            bits |= GL_COLOR_BUFFER_BIT;
        }
        if (params.depth) {
            if (*params.depth != m_clear_depth) {
                m_clear_depth = *params.depth;
                apply_clear_depth(m_clear_depth);
            }
            bits |= GL_DEPTH_BUFFER_BIT;
        }
        if (params.stencil) {
            if (*params.stencil != m_clear_stencil) {
                m_clear_stencil = *params.stencil;
                glClearStencil(m_clear_stencil);
            }
            bits |= GL_STENCIL_BUFFER_BIT;
        }

        if (bits == 0) {
            return;
        }

        // Clearing depth obeys the depth write mask like any draw, so a draw
        // that turned writes off would leave the old depth in place.
        if ((bits & GL_DEPTH_BUFFER_BIT) != 0 && !m_state.depth_write) {
            glDepthMask(GL_TRUE);
            m_state.depth_write = true;
        }
        glClear(bits);
    }

    auto gl::Device::set_viewport(int x, int y, int width, int height) noexcept -> void {
        const std::array next{x, y, width, height};
        if (next == m_viewport) {
            return;
        }

        flush();
        m_viewport = next;
        glViewport(x, y, width, height);
    }

    auto gl::Device::draw(
        const gl::Shader &shader,
        const gl::VertexArray &vertices,
        const DrawParams &params
    ) noexcept -> void {
        flush();

        TGX_ASSERT_MSG(vertices.vertex_count() > 0, "drawing from a vertex array with no vertex buffer");
        if constexpr (TGX_ENABLE_ASSERTS != 0) {
            check_vertex_inputs(shader, vertices);
            check_textures(shader, params);
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

        apply_state(m_state, params.state);
        tgx::detail::use_program(shader.id());
        for (std::uint32_t slot = 0; slot < gl::max_texture_slots; ++slot) {
            if (const Texture *texture = params.textures[slot]; texture != nullptr) {
                tgx::detail::bind_texture(slot, texture->id());
            }
        }
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
