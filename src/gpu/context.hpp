#pragma once

#include "tgx/core/device.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/image.hpp"
#include "tgx/core/render_target.hpp"

#include "gpu/batch.hpp"
#include "gpu/batch_gpu.hpp"
#include "gpu/clock_internal.hpp"
#include "gpu/gl_state.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace tgx::detail {
    // Coordinates the one Device's frame and queued Canvas geometry. All
    // drawing and resource mutations pass through here to preserve call order.
    // GL state and execution belong to GlState. Main thread only.
    class Context {
    public:
        // For a context just made current: resets GL state, makes the Canvas's
        // GPU resources and swaps one blank frame without counting it.
        [[nodiscard]] auto open() -> Result<void>;

        // Drops waiting shapes and GPU resources, then forgets all state.
        auto close() noexcept -> void;

        // Flushes, swaps buffers, then updates timing and publishes stats.
        auto present() noexcept -> void;

        auto restart_clock() noexcept -> void { clock_.restart(); }
        [[nodiscard]] auto timing() const noexcept -> const FrameClock & { return clock_; }
        [[nodiscard]] auto stats() const noexcept -> FrameStats { return last_frame_; }

        [[nodiscard]] auto has_depth(const Target &target) const noexcept -> bool {
            return gl_state_.has_depth(target);
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

        // Draw the batch first. Draw's arguments are checked by its callers.
        auto clear(const ClearCall &call) noexcept -> void;
        auto draw(const DrawCall &call) noexcept -> void;
        [[nodiscard]] auto read(const Target &target) -> Image;

        // For resource setup; changes tracked GL bindings.
        auto bind_framebuffer(GlId next) noexcept -> void { gl_state_.bind_framebuffer(next); }
        auto bind_vertex_array(GlId next) noexcept -> void { gl_state_.bind_vertex_array(next); }

        // Flush waiting uses before changing a program's uniforms or pixels.
        auto edit_program(GlId id) noexcept -> void;
        auto edit_texture(GlId texture) noexcept -> void;

        // Flush waiting uses, then forget bindings before GL deletes the id.
        auto before_delete_framebuffer(GlId id) noexcept -> void;
        auto before_delete_program(GlId id) noexcept -> void;
        auto before_delete_texture(GlId texture) noexcept -> void;

        auto forget_vertex_array(GlId id) noexcept -> void { gl_state_.forget_vertex_array(id); }

    private:
        friend auto context() noexcept -> Context &;

        Context() noexcept = default;
        ~Context() = default;
        Context(const Context &) = delete;
        auto operator=(const Context &) -> Context & = delete;
        Context(Context &&) = delete;
        auto operator=(Context &&) -> Context & = delete;

        GlState gl_state_{};
        FrameClock clock_{};
        FrameStats last_frame_{};
        int canvas_draws_{0};

        // Disengaged during setup and teardown: their resources notify this
        // Context. Drop the geometry before destroying the GPU resources.
        std::optional<BatchGpu> batch_gpu_{};
        std::optional<Batch> batch_{};
    };

    [[nodiscard]] auto context() noexcept -> Context &;
}
