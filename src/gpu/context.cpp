#include "gpu/context.hpp"

#include "tgx/core/assert.hpp"
#include "tgx/core/color.hpp"

#include "gpu/batch.hpp"
#include "gpu/batch_gpu.hpp"
#include "platform/window_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace tgx::detail {
    auto Context::open() -> Result<void> {
        close();
        gl_state_.reset();

        auto made = BatchGpu::create();
        if (!made) {
            close();
            return std::unexpected{made.error()};
        }
        batch_.emplace();
        batch_gpu_.emplace(std::move(*made));

        // On Wayland with a fractional scale, the buffer gets the size GLFW
        // reports only at a swap. Bootstrap with a blank frame so the first
        // app frame has the right size, without ticking its clock or stats.
        clear({.color = colors::black});
        swap_buffers();
        static_cast<void>(gl_state_.take_stats());
        return {};
    }

    auto Context::close() noexcept -> void {
        batch_.reset();
        batch_gpu_.reset();
        gl_state_ = {};
        clock_ = {};
        last_frame_ = {};
        canvas_draws_ = 0;
    }

    auto Context::present() noexcept -> void {
        flush();
        swap_buffers();
        clock_.tick();
        last_frame_ = gl_state_.take_stats();
        last_frame_.canvas_draws = std::exchange(canvas_draws_, 0);
    }

    auto Context::reserve_geometry(
        const BatchState &state,
        std::size_t vertex_count,
        std::size_t index_count
    ) noexcept -> std::pair<Batch &, std::uint16_t> {
        TGX_ASSERT(vertex_count <= batch_max_vertices && index_count <= batch_max_indices);
        if (!batch_->can_append(state, vertex_count, index_count)) {
            flush();
        }
        return {*batch_, batch_->start(state)};
    }

    auto Context::flush() noexcept -> void {
        if (!batch_ || batch_->empty()) {
            return;
        }
        gl_state_.draw(batch_gpu_->prepare_draw(*batch_, gl_state_));
        ++canvas_draws_;
        batch_->clear();
    }

    auto Context::clear(const ClearCall &call) noexcept -> void {
        flush();
        gl_state_.clear(call);
    }

    auto Context::draw(const DrawCall &call) noexcept -> void {
        flush();
        gl_state_.draw(call);
    }

    auto Context::read(const Target &target) -> Image {
        flush();
        return gl_state_.read(target);
    }

    auto Context::edit_program(GlId id) noexcept -> void {
        if (batch_ && batch_->uses_shader(id)) {
            flush();
        }
        gl_state_.use_program(id);
    }

    auto Context::edit_texture(GlId texture) noexcept -> void {
        if (batch_ && batch_->uses_texture(texture)) {
            flush();
        }
        // Any slot would do: no binding target is free of draws.
        gl_state_.bind_texture(0, texture);
    }

    auto Context::before_delete_framebuffer(GlId id) noexcept -> void {
        if (batch_ && batch_->uses_target(id)) {
            flush();
        }
        gl_state_.forget_framebuffer(id);
    }

    auto Context::before_delete_program(GlId id) noexcept -> void {
        if (batch_ && batch_->uses_shader(id)) {
            flush();
        }
        gl_state_.forget_program(id);
    }

    auto Context::before_delete_texture(GlId texture) noexcept -> void {
        if (batch_ && batch_->uses_texture(texture)) {
            flush();
        }
        gl_state_.forget_texture(texture);
    }

    auto context() noexcept -> Context & {
        static Context current{};
        return current;
    }
}
