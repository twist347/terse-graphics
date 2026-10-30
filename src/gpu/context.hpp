#pragma once

#include "tgx/core/color.hpp"
#include "tgx/core/device.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/image.hpp"
#include "tgx/core/render_target.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/vertex_array.hpp"

#include "gpu/clock_internal.hpp"
#include "gpu/surface.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace tgx::detail {
    class Batch;

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
        // Pixels outside it are not drawn. From the top-left, as viewport.
        // The scissor test is on only for the draw, as raw GL expects it off.
        std::optional<gl::Viewport> scissor{};
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
        FrameClock clock{};

        // Counted for the frame being drawn, which present() moves to
        // last_frame.
        FrameStats frame{};
        FrameStats last_frame{};

        // Null during setup and teardown: its own resources notify this
        // Context. unique_ptr::reset clears it before deleting the batch.
        std::unique_ptr<Batch> batch{};

        // For a context just made current: forgets tracked state, sets GL to
        // match and makes the Canvas's batch. A GL context can outlive a
        // Device and keep what it set.
        [[nodiscard]] auto open() -> Result<void>;

        // Drops the batch's shapes (their frame is over) and all state.
        auto close() noexcept -> void;

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
        auto bind_framebuffer(GlId next) noexcept -> void;

        // Flushes waiting draws and forgets the cached framebuffer.
        auto before_delete_framebuffer(GlId id) noexcept -> void;

        // Unless already current.
        auto use_program(GlId next) noexcept -> void;

        // Makes the program current to set a uniform, after drawing the
        // shapes waiting with the old values.
        auto edit_program(GlId id) noexcept -> void;

        // Flushes waiting uses and forgets the cached program.
        auto before_delete_program(GlId id) noexcept -> void;

        // Unless already bound.
        auto bind_vertex_array(GlId next) noexcept -> void;

        // Call before deleting a vertex array. GL unbinds it and may reuse
        // the id.
        auto forget_vertex_array(GlId id) noexcept -> void;

        // Unless already bound there. Leaves the slot active. slot must be
        // below gl::max_texture_slots.
        auto bind_texture(std::uint32_t slot, GlId texture) noexcept -> void;

        // Binds the texture to change it, after drawing the shapes waiting
        // with the old pixels. The next draw rebinds what it needs.
        auto edit_texture(GlId texture) noexcept -> void;

        // Flushes waiting reads and writes and forgets cached bindings.
        auto before_delete_texture(GlId texture) noexcept -> void;
    };

    [[nodiscard]] auto context() noexcept -> Context &;
}
