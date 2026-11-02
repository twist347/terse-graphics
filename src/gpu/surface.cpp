#include "gpu/surface.hpp"

#include "tgx/core/render_target.hpp"

#include "platform/window_internal.hpp"

namespace tgx::detail {
    auto surface_of(const Target &target) noexcept -> Surface {
        if (target.framebuffer == 0) {
            return {.pixels = framebuffer_size(), .units = window_size()};
        }
        return {.pixels = target.size, .units = target.size};
    }
}
