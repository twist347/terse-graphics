#pragma once

#include "tgx/core/math.hpp"

#include <cmath>

namespace tgx {
    namespace detail {
        [[nodiscard]] inline auto valid_camera_transform(float zoom, float rotation) noexcept -> bool {
            return zoom != 0.f && std::isfinite(zoom) && std::isfinite(rotation);
        }
    }

    // Shows the world point target at the canvas point offset, rotated and
    // zoomed around it. The default camera changes nothing.
    //
    //     canvas.set_camera({.target = player, .offset = {640, 360}, .zoom = 2.f});
    struct Camera2D {
        Vec2 target{};
        // Usually the canvas center, to keep target centered.
        Vec2 offset{};
        // Radians, clockwise.
        float rotation{0.f};
        // Finite and nonzero; a negative zoom mirrors both axes.
        float zoom{1.f};

        [[nodiscard]] constexpr auto operator==(const Camera2D &) const noexcept -> bool = default;

        // To canvas coordinates. Canvas::to_screen goes on to the window.
        [[nodiscard]] auto to_screen(Vec2 world) const noexcept -> Vec2 {
            TGX_ASSERT_MSG(detail::valid_camera_transform(zoom, rotation), "camera zoom {} and rotation {}", zoom, rotation);
            return rotate((world - target) * zoom, rotation) + offset;
        }

        // From canvas coordinates. For a window point, such as the mouse,
        // use Canvas::to_world, which accounts for the viewport and size.
        [[nodiscard]] auto to_world(Vec2 point) const noexcept -> Vec2 {
            TGX_ASSERT_MSG(detail::valid_camera_transform(zoom, rotation), "camera zoom {} and rotation {}", zoom, rotation);
            return rotate(point - offset, -rotation) / zoom + target;
        }

        // to_screen as a matrix, for a shader.
        [[nodiscard]] auto matrix() const noexcept -> Mat4 {
            TGX_ASSERT_MSG(detail::valid_camera_transform(zoom, rotation), "camera zoom {} and rotation {}", zoom, rotation);
            const float c = std::cos(rotation) * zoom;
            const float s = std::sin(rotation) * zoom;
            const Vec2 moved = offset - Vec2{target.x * c - target.y * s, target.x * s + target.y * c};

            Mat4 m{};
            m.columns[0] = {c, s, 0.f, 0.f};
            m.columns[1] = {-s, c, 0.f, 0.f};
            m.columns[3] = {moved.x, moved.y, 0.f, 1.f};
            return m;
        }
    };
}
