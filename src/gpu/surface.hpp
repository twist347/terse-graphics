#pragma once

#include "tgx/core/math.hpp"

#include "tgx/gl/draw.hpp"

namespace tgx::detail {
    struct Target;

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
}
