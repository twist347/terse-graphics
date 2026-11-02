#pragma once

#include "tgx/core/color.hpp"
#include "tgx/core/device.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/image.hpp"
#include "tgx/core/render_target.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/vertex_array.hpp"

#include "gpu/batch.hpp"
#include "gpu/clock_internal.hpp"
#include "gpu/surface.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace tgx::detail {
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
    class Context {
    public:
        // For a context just made current: forgets tracked state, sets GL to
        // match, makes the Canvas's batch and swaps one blank frame without
        // counting it. A GL context can outlive a Device and keep what it set.
        [[nodiscard]] auto open() -> Result<void>;

        // Drops the batch's shapes (their frame is over) and all state.
        auto close() noexcept -> void;

        // Flushes, swaps buffers, then updates timing and publishes stats.
        auto present() noexcept -> void;

        auto restart_clock() noexcept -> void { clock_.restart(); }

        [[nodiscard]] auto timing() const noexcept -> const FrameClock & { return clock_; }

        [[nodiscard]] auto stats() const noexcept -> FrameStats { return last_frame_; }

        [[nodiscard]] auto has_depth(const Target &target) const noexcept -> bool {
            return target.framebuffer == 0 ? window_depth_bits_ > 0 : target.depth;
        }

        // Makes room with the requested state. Push exactly these counts
        // into the returned batch before the next Context operation.
        [[nodiscard]] auto reserve_geometry(
            const BatchState &state,
            std::size_t vertex_count,
            std::size_t index_count
        ) noexcept -> std::pair<Batch &, std::uint16_t>;

        // Draws the batch, if any.
        auto flush() noexcept -> void;

        // Draws the batch first. Clears the whole target.
        auto clear(const ClearCall &call) noexcept -> void;

        // Draws the batch first. Checks nothing; callers do.
        auto draw(const DrawCall &call) noexcept -> void;

        // Draws the batch first. Rows top to bottom (GL gives them bottom up).
        [[nodiscard]] auto read(const Target &target) -> Image;

        // Unless already bound.
        auto bind_framebuffer(GlId next) noexcept -> void;

        // Flushes waiting draws and forgets the cached framebuffer.
        auto before_delete_framebuffer(GlId id) noexcept -> void;

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

        // Binds the texture to change it, after drawing the shapes waiting
        // with the old pixels. The next draw rebinds what it needs.
        auto edit_texture(GlId texture) noexcept -> void;

        // Flushes waiting reads and writes and forgets cached bindings.
        auto before_delete_texture(GlId texture) noexcept -> void;

    private:
        friend auto context() noexcept -> Context &;
        friend auto Batch::flush(Context &context) noexcept -> void;

        Context() noexcept = default;
        ~Context() = default;
        Context(const Context &) = delete;
        auto operator=(const Context &) -> Context & = delete;
        Context(Context &&) noexcept = default;
        auto operator=(Context &&) noexcept -> Context & = default;

        // These skip flushing: only Context and the batch currently being
        // flushed may use them.
        auto submit(const DrawCall &call) noexcept -> void;
        auto submit_batch(const DrawCall &call) noexcept -> void;
        auto use_program(GlId next) noexcept -> void;

        // Unless already bound there. Leaves the slot active. slot must be
        // below gl::max_texture_slots.
        auto bind_texture(std::uint32_t slot, GlId texture) noexcept -> void;

        // The default framebuffer's actual depth, queried at open().
        int window_depth_bits_{0};
        gl::RenderState state_{};
        Color clear_color_{0, 0, 0, 0};
        float clear_depth_{1.f};
        // As passed to glViewport: from the bottom-left, unlike every other
        // gl::Viewport in tgx.
        gl::Viewport gl_viewport_{};
        // Bound for draws and clears. 0: the window.
        GlId framebuffer_{0};
        // 0: none.
        GlId program_{0};
        // 0: none.
        GlId vertex_array_{0};
        // Texture in each slot (0: none), and the slot last made active.
        std::array<GlId, gl::max_texture_slots> textures_{};
        std::uint32_t active_slot_{0};

        // Timing of presented frames, read by Clock.
        FrameClock clock_{};

        // Counted for the frame being drawn, which present() moves to
        // last_frame_.
        FrameStats frame_{};
        FrameStats last_frame_{};

        // Null during setup and teardown: its own resources notify this
        // Context. unique_ptr::reset clears it before deleting the batch.
        std::unique_ptr<Batch> batch_{};
    };

    [[nodiscard]] auto context() noexcept -> Context &;
}
