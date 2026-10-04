#pragma once

#include "tgx/math.h"

#include <cmath>

namespace tgx {
    // A 2D view of the world for the Canvas: the world point target appears at
    // the canvas point offset, turned by rotation and scaled by zoom around it.
    // The default one shows the world as it is, one unit to one canvas unit.
    //
    //     canvas.set_camera({.target = player, .offset = {640, 360}, .zoom = 2.f});
    struct Camera2D {
        Vec2 target{};
        // Usually the middle of the canvas, to keep target there.
        Vec2 offset{};
        // Radians, clockwise on screen, like Canvas rotations.
        float rotation{0.f};
        float zoom{1.f};

        [[nodiscard]] constexpr auto operator==(const Camera2D &) const noexcept -> bool = default;

        // Where a world point shows up on the canvas. Canvas::to_screen goes
        // on to the window.
        [[nodiscard]] auto to_screen(Vec2 world) const noexcept -> Vec2 {
            const float c = std::cos(rotation);
            const float s = std::sin(rotation);
            const Vec2 v = (world - target) * zoom;
            return Vec2{v.x * c - v.y * s, v.x * s + v.y * c} + offset;
        }

        // The world point under a point of the canvas. For a point of the
        // window, such as the mouse, Canvas::to_world: it also knows where the
        // canvas lies in the window and how it is stretched.
        [[nodiscard]] auto to_world(Vec2 point) const noexcept -> Vec2 {
            const float c = std::cos(rotation);
            const float s = std::sin(rotation);
            const Vec2 v = point - offset;
            return Vec2{v.x * c + v.y * s, -v.x * s + v.y * c} / zoom + target;
        }

        // The same as to_screen, as a matrix for a shader.
        [[nodiscard]] auto matrix() const noexcept -> Mat4 {
            const float c = std::cos(rotation) * zoom;
            const float s = std::sin(rotation) * zoom;
            const Vec2 moved = offset - Vec2{target.x * c - target.y * s, target.x * s + target.y * c};

            Mat4 m;
            m.columns[0] = {c, s, 0.f, 0.f};
            m.columns[1] = {-s, c, 0.f, 0.f};
            m.columns[3] = {moved.x, moved.y, 0.f, 1.f};
            return m;
        }
    };
}
