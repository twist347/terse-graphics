#include "gpu/gl_state.hpp"

#include "tgx/core/blend.hpp"
#include "tgx/core/color.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/math.hpp"

#include "base/log_internal.hpp"
#include "gpu/gl_pixels.hpp"
#include "gpu/surface.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(std::is_same_v<GLuint, tgx::GlId>);

namespace {
    auto apply_clear_color(tgx::Color color) noexcept -> void {
        const tgx::Vec4 unit = tgx::to_vec4(color);
        glClearColor(unit.x, unit.y, unit.z, unit.w);
    }

    auto apply_clear_depth(float depth) noexcept -> void {
        glClearDepth(static_cast<GLdouble>(depth));
    }

    // A rect from the top-left as GL takes it: from the bottom-left of a
    // framebuffer that tall.
    [[nodiscard]] constexpr auto from_bottom(tgx::gl::Viewport rect, int height) noexcept -> tgx::gl::Viewport {
        return {rect.x, height - rect.y - rect.height, rect.width, rect.height};
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
        GLenum src_rgb{};
        GLenum dst_rgb{};
        GLenum src_alpha{};
        GLenum dst_alpha{};
    };

    // Alpha stores coverage, building up like layers of paint, as a later
    // draw of this framebuffer as a texture expects.
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
            // Never sent to GL: culling is disabled instead.
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

    // For a setting whose none means a disabled GL capability: toggles the
    // capability on a change to or from none, and calls apply for any other
    // value. Does nothing if the setting is unchanged.
    template<typename Setting, typename Apply>
    auto apply_switched(Setting current, Setting next, GLenum capability, Apply apply) noexcept -> void {
        if (next == current) {
            return;
        }
        if ((next == Setting::none) != (current == Setting::none)) {
            set_enabled(capability, next != Setting::none);
        }
        if (next != Setting::none) {
            apply(next);
        }
    }

    // Touches only what differs.
    auto apply_state(tgx::gl::RenderState &current, const tgx::gl::RenderState &next) noexcept -> void {
        apply_switched(current.blend, next.blend, GL_BLEND, [](tgx::Blend blend) noexcept {
            const GlBlend factors = to_gl(blend);
            glBlendFuncSeparate(factors.src_rgb, factors.dst_rgb, factors.src_alpha, factors.dst_alpha);
        });

        apply_switched(current.depth, next.depth, GL_DEPTH_TEST, [](tgx::gl::Depth depth) noexcept {
            glDepthFunc(to_gl(depth));
        });

        if (next.depth_write != current.depth_write) {
            glDepthMask(next.depth_write ? GL_TRUE : GL_FALSE);
        }

        apply_switched(current.cull, next.cull, GL_CULL_FACE, [](tgx::gl::Cull cull) noexcept {
            glCullFace(to_gl(cull));
        });

        if (next.fill != current.fill) {
            glPolygonMode(GL_FRONT_AND_BACK, to_gl(next.fill));
        }

        current = next;
    }
}

namespace tgx::detail {
    auto GlState::reset() noexcept -> void {
        *this = {};

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        // Off by default, which keeps points at 1 pixel whatever the shader
        // writes to gl_PointSize. Never disabled again.
        glEnable(GL_PROGRAM_POINT_SIZE);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(0);
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        GLint depth_type = GL_NONE;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &depth_type);
        // A size query on an absent attachment would be GL_INVALID_OPERATION.
        if (depth_type != GL_NONE) {
            GLint depth_bits = 0;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depth_bits);
            window_depth_bits_ = depth_bits;
        }
        log_info("  depth:    {} bits", window_depth_bits_);
        apply_clear_color(clear_color_);
        apply_clear_depth(clear_depth_);

        // Set, not assumed: draws change the viewport only when theirs
        // differs. The whole framebuffer is the same from either corner.
        gl_viewport_ = surface_of({}).viewport();
        glViewport(0, 0, gl_viewport_.width, gl_viewport_.height);
    }

    auto GlState::clear(const ClearCall &call) noexcept -> void {
        GLbitfield bits = 0;

        if (call.color) {
            // A render target holds premultiplied colors, as Blend::alpha
            // leaves them; a straight transparent color would show brighter.
            if (const Color color = call.target.framebuffer != 0 ? call.color->premultiplied() : *call.color;
                color != clear_color_) {
                clear_color_ = color;
                apply_clear_color(clear_color_);
            }
            bits |= GL_COLOR_BUFFER_BIT;
        }
        if (call.depth) {
            if (*call.depth != clear_depth_) {
                clear_depth_ = *call.depth;
                apply_clear_depth(clear_depth_);
            }
            // Clearing depth obeys the depth write mask, which a draw may
            // have turned off.
            if (!state_.depth_write) {
                glDepthMask(GL_TRUE);
                state_.depth_write = true;
            }
            bits |= GL_DEPTH_BUFFER_BIT;
        }

        if (bits == 0) {
            return;
        }
        // glClear ignores the viewport.
        bind_framebuffer(call.target.framebuffer);
        glClear(bits);
        ++work_.clears;
    }

    auto GlState::draw(const DrawCall &call) noexcept -> void {
        if (call.count == 0) {
            return;
        }

        // Flipped with the target's size now, not when the draw was asked
        // for, so the top-left stays the top-left across a resize.
        const int height = surface_of(call.target).pixels.height;
        const gl::Viewport flipped = from_bottom(call.viewport, height);
        bind_framebuffer(call.target.framebuffer);
        if (flipped != gl_viewport_) {
            glViewport(flipped.x, flipped.y, flipped.width, flipped.height);
            gl_viewport_ = flipped;
        }
        if (call.scissor) {
            const gl::Viewport scissor = from_bottom(*call.scissor, height);
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
        }
        apply_state(state_, call.state);
        use_program(call.program);
        for (std::uint32_t slot = 0; slot < gl::max_texture_slots; ++slot) {
            if (call.textures[slot] != 0) {
                bind_texture(slot, call.textures[slot]);
            }
        }
        bind_vertex_array(call.vertex_array);

        const GLenum mode = to_gl(call.primitive);
        const auto count = static_cast<GLsizei>(call.count);

        ++work_.draws;
        if (!call.index_type) {
            glDrawArrays(mode, static_cast<GLint>(call.first), count);
        } else {
            const std::size_t index_size = *call.index_type == gl::IndexType::uint32
                                               ? sizeof(std::uint32_t)
                                               : sizeof(std::uint16_t);

            // GL takes the start as a byte offset into the index buffer,
            // passed as a pointer.
            const auto *start = reinterpret_cast<const void *>(call.first * index_size);
            glDrawElements(mode, count, to_gl(*call.index_type), start);
        }

        if (call.scissor) {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    auto GlState::read(const Target &target) -> Image {
        const Size size = surface_of(target).pixels;
        auto image = Image::create(size);
        if (image.empty()) {
            return image;
        }
        bind_framebuffer(target.framebuffer);
        glReadPixels(0, 0, size.width, size.height, pixel_format, pixel_type, image.pixels().data());

        const auto pixels = image.pixels();
        const auto width = static_cast<std::size_t>(size.width);
        for (std::size_t top = 0, bottom = static_cast<std::size_t>(size.height) - 1; top < bottom; ++top, --bottom) {
            std::ranges::swap_ranges(pixels.subspan(top * width, width), pixels.subspan(bottom * width, width));
        }
        return image;
    }

    auto GlState::bind_framebuffer(GlId next) noexcept -> void {
        if (next != framebuffer_) {
            glBindFramebuffer(GL_FRAMEBUFFER, next);
            framebuffer_ = next;
            ++work_.target_switches;
        }
    }

    auto GlState::forget_framebuffer(GlId id) noexcept -> void {
        if (id == framebuffer_) {
            framebuffer_ = 0;
        }
    }

    auto GlState::use_program(GlId next) noexcept -> void {
        if (next != program_) {
            glUseProgram(next);
            program_ = next;
            ++work_.shader_switches;
        }
    }

    auto GlState::forget_program(GlId id) noexcept -> void {
        if (id == program_) {
            program_ = 0;
        }
    }

    auto GlState::bind_vertex_array(GlId next) noexcept -> void {
        if (next != vertex_array_) {
            glBindVertexArray(next);
            vertex_array_ = next;
        }
    }

    auto GlState::forget_vertex_array(GlId id) noexcept -> void {
        if (id == vertex_array_) {
            vertex_array_ = 0;
        }
    }

    auto GlState::bind_texture(std::uint32_t slot, GlId texture) noexcept -> void {
        if (slot != active_slot_) {
            glActiveTexture(GL_TEXTURE0 + slot);
            active_slot_ = slot;
        }
        if (texture != textures_[slot]) {
            glBindTexture(GL_TEXTURE_2D, texture);
            textures_[slot] = texture;
            ++work_.texture_switches;
        }
    }

    auto GlState::forget_texture(GlId texture) noexcept -> void {
        std::ranges::replace(textures_, texture, GLuint{0});
    }
}
