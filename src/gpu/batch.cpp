#include "gpu/batch.hpp"

#include "tgx/core/assert.hpp"

#include <cstddef>
#include <cstdint>

namespace tgx::detail {
    Batch::Batch() {
        // Never filled past this, so adding a shape never allocates.
        vertices_.reserve(batch_max_vertices);
        indices_.reserve(batch_max_indices);
    }

    auto Batch::can_append(
        const BatchState &state,
        std::size_t vertex_count,
        std::size_t index_count
    ) const noexcept -> bool {
        return (empty() || state == state_)
               && vertex_count <= batch_max_vertices - vertices_.size()
               && index_count <= batch_max_indices - indices_.size();
    }

    auto Batch::start(const BatchState &state) noexcept -> std::uint16_t {
        TGX_ASSERT(empty() || state == state_);
        if (empty()) {
            state_ = state;
        }
        return static_cast<std::uint16_t>(vertices_.size());
    }

    auto Batch::clear() noexcept -> void {
        vertices_.clear();
        indices_.clear();
    }

    auto Batch::uses_texture(GlId texture) const noexcept -> bool {
        return !indices_.empty() && (state_.texture == texture || state_.target.texture == texture);
    }

    auto Batch::uses_shader(GlId program) const noexcept -> bool {
        return !indices_.empty() && state_.program == program;
    }

    auto Batch::uses_target(GlId framebuffer) const noexcept -> bool {
        return !indices_.empty() && state_.target.framebuffer == framebuffer;
    }
}
