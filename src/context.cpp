#include "context.h"

#include "tgx/blend.h"
#include "tgx/color.h"
#include "tgx/handle.h"
#include "tgx/math.h"

#include "tgx/gl/draw.h"
#include "tgx/gl/texture_slot.h"
#include "tgx/gl/vertex_array.h"

#include "batch.h"
#include "window_internal.h"

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(std::is_same_v<GLuint, tgx::GlId>);

namespace {
    tgx::detail::Context s_context;

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
}

namespace tgx {
    auto detail::context() noexcept -> Context & {
        return s_context;
    }

    auto detail::surface_of(const Target &target) noexcept -> Surface {
        if (target.framebuffer == 0) {
            return {framebuffer_size(), window_size()};
        }
        return {target.size, target.size};
    }

    auto detail::flush_texture_use(GlId texture) noexcept -> void {
        if (s_context.batch && s_context.batch->uses_texture(texture)) {
            s_context.batch->flush(s_context);
        }
    }

    auto detail::flush_shader_use(GlId program) noexcept -> void {
        if (s_context.batch && s_context.batch->uses_shader(program)) {
            s_context.batch->flush(s_context);
        }
    }

    auto detail::flush_target_use(GlId framebuffer) noexcept -> void {
        if (s_context.batch && s_context.batch->uses_target(framebuffer)) {
            s_context.batch->flush(s_context);
        }
    }

    auto detail::Context::flush() noexcept -> void {
        if (batch) {
            batch->flush(*this);
        }
    }

    auto detail::Context::clear(const Target &target, const ClearParams &params) noexcept -> void {
        flush();

        GLbitfield bits = 0;

        if (params.color) {
            if (*params.color != clear_color) {
                clear_color = *params.color;
                apply_clear_color(clear_color);
            }
            bits |= GL_COLOR_BUFFER_BIT;
        }
        if (params.depth) {
            if (*params.depth != clear_depth) {
                clear_depth = *params.depth;
                apply_clear_depth(clear_depth);
            }
            bits |= GL_DEPTH_BUFFER_BIT;
        }
        if (params.stencil) {
            if (*params.stencil != clear_stencil) {
                clear_stencil = *params.stencil;
                glClearStencil(clear_stencil);
            }
            bits |= GL_STENCIL_BUFFER_BIT;
        }

        if (bits == 0) {
            return;
        }

        // Clearing depth obeys the depth write mask like any draw, so a draw
        // that turned writes off would leave the old depth in place.
        if ((bits & GL_DEPTH_BUFFER_BIT) != 0 && !state.depth_write) {
            glDepthMask(GL_TRUE);
            state.depth_write = true;
        }
        // The whole of it: glClear ignores the viewport.
        bind_framebuffer(target.framebuffer);
        glClear(bits);
    }

    auto detail::Context::draw(const DrawCall &call) noexcept -> void {
        if (call.count == 0) {
            return;
        }

        // Turned here, against the target as it is when drawn into, not when
        // the draw was asked for: a rectangle from the top-left stays at the
        // top-left even if the window was resized in between.
        const gl::Viewport flipped{
            call.viewport.x,
            surface_of(call.target).pixels.height - call.viewport.y - call.viewport.height,
            call.viewport.width,
            call.viewport.height,
        };
        bind_framebuffer(call.target.framebuffer);
        if (flipped != gl_viewport) {
            glViewport(flipped.x, flipped.y, flipped.width, flipped.height);
            gl_viewport = flipped;
        }
        apply_state(state, call.state);
        use_program(call.program);
        for (std::uint32_t slot = 0; slot < gl::max_texture_slots; ++slot) {
            if (call.textures[slot] != 0) {
                bind_texture(slot, call.textures[slot]);
            }
        }
        glBindVertexArray(call.vertex_array);

        const GLenum mode = to_gl(call.primitive);
        const auto count = static_cast<GLsizei>(call.count);

        if (!call.index_type) {
            glDrawArrays(mode, static_cast<GLint>(call.first), count);
            return;
        }

        const std::size_t index_size = *call.index_type == gl::IndexType::uint32 ? 4 : 2;

        // GL takes the start of an indexed draw as a byte offset into the index
        // buffer, passed where a pointer used to go.
        const auto *start = reinterpret_cast<const void *>(call.first * index_size);
        glDrawElements(mode, count, to_gl(*call.index_type), start);
    }

    auto detail::Context::bind_framebuffer(GlId next) noexcept -> void {
        if (next != framebuffer) {
            glBindFramebuffer(GL_FRAMEBUFFER, next);
            framebuffer = next;
        }
    }

    auto detail::Context::forget_framebuffer(GlId id) noexcept -> void {
        if (id == framebuffer) {
            framebuffer = 0;
        }
    }

    auto detail::Context::use_program(GlId next) noexcept -> void {
        if (next != program) {
            glUseProgram(next);
            program = next;
        }
    }

    auto detail::Context::forget_program(GlId id) noexcept -> void {
        if (id == program) {
            program = 0;
        }
    }

    auto detail::Context::bind_texture(std::uint32_t slot, GlId texture) noexcept -> void {
        TGX_ASSERT(slot < gl::max_texture_slots);

        if (slot != active_slot) {
            glActiveTexture(GL_TEXTURE0 + slot);
            active_slot = slot;
        }
        if (texture != textures[slot]) {
            glBindTexture(GL_TEXTURE_2D, texture);
            textures[slot] = texture;
        }
    }

    auto detail::Context::forget_texture(GlId texture) noexcept -> void {
        std::ranges::replace(textures, texture, GLuint{0});
    }

    auto detail::Context::reset() noexcept -> void {
        *this = {};

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        apply_clear_color(clear_color);
        apply_clear_depth(clear_depth);
        glClearStencil(clear_stencil);

        // Set rather than assumed, like the rest: draws only change the
        // viewport when theirs differs from this. The whole framebuffer is the
        // same from either corner.
        gl_viewport = surface_of({}).viewport();
        glViewport(0, 0, gl_viewport.width, gl_viewport.height);
    }
}
