#pragma once

#include "tgx/core/color.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/image.hpp"
#include "tgx/core/math.hpp"
#include "tgx/core/render_target.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/vertex_array.hpp"

#include "core/clock_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace tgx::detail {
    class Batch;

    // Sizes of the window or render target being drawn into. The only place
    // that tells the window from a render target.
    struct Surface {
        // Viewports are measured in pixels.
        Size pixels{};
        // In the coordinates of a canvas covering all of it. Differs from
        // pixels on a scaling display.
        Size units{};

        // The whole surface.
        [[nodiscard]] constexpr auto viewport() const noexcept -> gl::Viewport {
            return {0, 0, pixels.width, pixels.height};
        }
    };

    [[nodiscard]] auto surface_of(const Target &target) noexcept -> Surface;

    // One clear, with GL ids, from Device::clear or Canvas::clear. Empty
    // fields are not cleared.
    struct ClearCall {
        Target target{};
        std::optional<Color> color{};
        std::optional<float> depth{};
    };

    // One draw, with GL ids, from Device::draw or the batch.
    struct DrawCall {
        Target target{};
        GlId program{0};
        GlId vertex_array{0};
        // Empty: no indices.
        std::optional<gl::IndexType> index_type{};
        // In vertices, or in indices when index_type is set.
        std::size_t first{0};
        std::size_t count{0};
        gl::Primitive primitive{gl::Primitive::triangles};
        gl::RenderState state{};
        // From the top-left. Flipped for GL at draw time, with the target's
        // size at that moment, as GL itself applies a viewport.
        gl::Viewport viewport{};
        // 0 leaves the slot unchanged.
        std::array<GlId, gl::max_texture_slots> textures{};
    };

    // The GL context while a Device owns it. Tracks GL state, so setting what
    // is already set costs nothing, and holds the Canvas's batch. Main thread
    // only.
    struct Context {
        gl::RenderState state{};
        Color clear_color{0, 0, 0, 0};
        float clear_depth{1.f};
        // As passed to glViewport: from the bottom-left, unlike every other
        // gl::Viewport in tgx.
        gl::Viewport gl_viewport{};
        // Bound for draws and clears. 0: the window.
        GlId framebuffer{0};
        // 0: none.
        GlId program{0};
        // 0: none.
        GlId vertex_array{0};
        // Texture in each slot (0: none), and the slot last made active.
        std::array<GlId, gl::max_texture_slots> textures{};
        std::uint32_t active_slot{0};

        // Timing of presented frames, read by Clock.
        FrameClock clock;

        // Null while the batch is created and destroyed, when its own shader
        // and texture call the flush hooks; they skip a null batch. unique_ptr
        // is nulled before it deletes; optional does not promise that.
        std::unique_ptr<Batch> batch;

        // For a context just made current: forgets tracked state and sets GL
        // to match. A GL context can outlive a Device and keep what it set.
        // Leaves no batch.
        auto reset() noexcept -> void;

        // Draws the batch, if any.
        auto flush() noexcept -> void;

        // Draws the batch first. Clears the whole target.
        auto clear(const ClearCall &call) noexcept -> void;

        // Draws the batch first. Checks nothing; callers do.
        auto draw(const DrawCall &call) noexcept -> void;

        // Without drawing the batch first. The batch draws itself with it.
        auto submit(const DrawCall &call) noexcept -> void;

        // Draws the batch first. Rows top to bottom (GL gives them bottom up).
        [[nodiscard]] auto read(const Target &target) -> Image;

        // Unless already bound.
        auto bind_framebuffer(GlId framebuffer) noexcept -> void;

        // Call before deleting a framebuffer. If bound, GL falls back to the
        // window's, and GL may reuse the id.
        auto forget_framebuffer(GlId framebuffer) noexcept -> void;

        // Unless already current.
        auto use_program(GlId program) noexcept -> void;

        // Call before deleting a program. GL may reuse the id, which must not
        // be mistaken for the current one.
        auto forget_program(GlId program) noexcept -> void;

        // Unless already bound.
        auto bind_vertex_array(GlId vertex_array) noexcept -> void;

        // Call before deleting a vertex array. GL unbinds it and may reuse
        // the id.
        auto forget_vertex_array(GlId vertex_array) noexcept -> void;

        // Unless already bound there. Leaves the slot active. slot must be
        // below gl::max_texture_slots.
        auto bind_texture(std::uint32_t slot, GlId texture) noexcept -> void;

        // Call before deleting a texture. GL unbinds it from every slot and
        // may reuse the id.
        auto forget_texture(GlId texture) noexcept -> void;
    };

    [[nodiscard]] auto context() noexcept -> Context &;

    // Call before a texture or program changes or is deleted. If the batch
    // uses it, the batch is drawn now, so it never sees later changes.
    auto flush_texture_use(GlId texture) noexcept -> void;

    auto flush_shader_use(GlId program) noexcept -> void;

    // Call before a render target is deleted: draws the shapes waiting for
    // it. Whichever of its framebuffer and texture goes first draws them
    // (flush_texture_use covers the texture).
    auto flush_target_use(GlId framebuffer) noexcept -> void;
}
