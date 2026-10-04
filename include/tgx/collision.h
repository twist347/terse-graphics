#pragma once

#include "tgx/math.h"

#include <algorithm>
#include <optional>

namespace tgx {
    struct Circle {
        Vec2 center{};
        float radius{0.f};

        [[nodiscard]] constexpr auto operator==(const Circle &) const noexcept -> bool = default;
    };

    // Checks for 2D games: whether a point is inside a shape, whether two
    // shapes overlap, where they meet. Rects are taken with sizes of zero or
    // more, as the Canvas draws them.

    // Its left and top edges are inside, its right and bottom edges outside,
    // so a point on the line between two tiles is in exactly one of them.
    [[nodiscard]] constexpr auto contains(Rect rect, Vec2 point) noexcept -> bool {
        return point.x >= rect.x && point.x < rect.x + rect.width
            && point.y >= rect.y && point.y < rect.y + rect.height;
    }

    // The edge counts as inside.
    [[nodiscard]] constexpr auto contains(Circle circle, Vec2 point) noexcept -> bool {
        const Vec2 d = point - circle.center;
        return dot(d, d) <= circle.radius * circle.radius;
    }

    // The triangle a b c, either way round; the edges count as inside.
    [[nodiscard]] constexpr auto contains(Vec2 a, Vec2 b, Vec2 c, Vec2 point) noexcept -> bool {
        // Which side of each edge the point is on: inside it is the same side
        // of all three, or on one of them.
        const float ab = cross(b - a, point - a);
        const float bc = cross(c - b, point - b);
        const float ca = cross(a - c, point - c);
        const bool any_negative = ab < 0.f || bc < 0.f || ca < 0.f;
        const bool any_positive = ab > 0.f || bc > 0.f || ca > 0.f;
        return !(any_negative && any_positive);
    }

    // Shapes that only touch, edge to edge, do not overlap: two tiles side by
    // side are not in each other.
    [[nodiscard]] constexpr auto overlaps(Rect a, Rect b) noexcept -> bool {
        return a.x < b.x + b.width && b.x < a.x + a.width
            && a.y < b.y + b.height && b.y < a.y + a.height;
    }

    [[nodiscard]] constexpr auto overlaps(Circle a, Circle b) noexcept -> bool {
        const Vec2 d = b.center - a.center;
        const float reach = a.radius + b.radius;
        return dot(d, d) < reach * reach;
    }

    [[nodiscard]] constexpr auto overlaps(Rect rect, Circle circle) noexcept -> bool {
        // The rect's point nearest the circle's center.
        const Vec2 nearest{
            std::clamp(circle.center.x, rect.x, rect.x + rect.width),
            std::clamp(circle.center.y, rect.y, rect.y + rect.height),
        };
        const Vec2 d = circle.center - nearest;
        return dot(d, d) < circle.radius * circle.radius;
    }

    [[nodiscard]] constexpr auto overlaps(Circle circle, Rect rect) noexcept -> bool {
        return overlaps(rect, circle);
    }

    // The part two rects share; an empty Rect when they do not overlap. Its
    // width and height are how far one is into the other, e.g. how far to push
    // a player back out of a wall.
    [[nodiscard]] constexpr auto intersection(Rect a, Rect b) noexcept -> Rect {
        const float left = std::max(a.x, b.x);
        const float top = std::max(a.y, b.y);
        const float right = std::min(a.x + a.width, b.x + b.width);
        const float bottom = std::min(a.y + a.height, b.y + b.height);
        if (right <= left || bottom <= top) {
            return {};
        }
        return {left, top, right - left, bottom - top};
    }

    // Where the segments from a1 to a2 and from b1 to b2 cross, ends included.
    // Parallel segments have no single such point, so none, even if they lie
    // along each other.
    [[nodiscard]] constexpr auto intersection(Vec2 a1, Vec2 a2, Vec2 b1, Vec2 b2) noexcept -> std::optional<Vec2> {
        const Vec2 a = a2 - a1;
        const Vec2 b = b2 - b1;
        const float denominator = cross(a, b);
        if (denominator == 0.f) {
            return std::nullopt;
        }
        // How far along each segment the lines cross, 0 at its start, 1 at
        // its end.
        const Vec2 start = b1 - a1;
        const float t = cross(start, b) / denominator;
        const float u = cross(start, a) / denominator;
        if (t < 0.f || t > 1.f || u < 0.f || u > 1.f) {
            return std::nullopt;
        }
        return a1 + a * t;
    }
}
