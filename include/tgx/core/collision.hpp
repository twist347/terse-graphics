#pragma once

#include "tgx/core/assert.hpp"
#include "tgx/core/math.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>

namespace tgx {
    struct Circle {
        Vec2 center{};
        float radius{0.f};

        [[nodiscard]] constexpr auto operator==(const Circle &) const noexcept -> bool = default;
    };

    // Collision checks: point in shape, shapes overlapping, where they meet.
    // Rect sizes and circle radii must be zero or more (asserted). A rect with
    // no area contains no point, overlaps nothing, and intersects to empty.

    namespace detail {
        // Asserted by the callers, so the message names them.
        [[nodiscard]] constexpr auto valid(Rect rect) noexcept -> bool { return rect.width >= 0.f && rect.height >= 0.f; }

        // Zero or more and finite; also used by the Canvas. Not
        // std::isfinite, to stay constexpr on every compiler.
        [[nodiscard]] constexpr auto valid_radius(float radius) noexcept -> bool {
            return radius >= 0.f && radius <= std::numeric_limits<float>::max();
        }

        [[nodiscard]] constexpr auto valid(Circle circle) noexcept -> bool { return valid_radius(circle.radius); }
    }

    // Left and top edges are inside, right and bottom outside, so a point
    // between two tiles is in exactly one.
    [[nodiscard]] constexpr auto contains(Rect rect, Vec2 point) noexcept -> bool {
        TGX_ASSERT_MSG(detail::valid(rect), "rect of size {}x{}", rect.width, rect.height);

        return point.x >= rect.x && point.x < rect.right()
               && point.y >= rect.y && point.y < rect.bottom();
    }

    // The edge counts as inside.
    [[nodiscard]] constexpr auto contains(Circle circle, Vec2 point) noexcept -> bool {
        TGX_ASSERT_MSG(detail::valid(circle), "circle of radius {}", circle.radius);

        const Vec2 d = point - circle.center;
        return length_squared(d) <= circle.radius * circle.radius;
    }

    // The triangle a b c, either winding. The edges count as inside. A
    // triangle with no area has no inside.
    [[nodiscard]] constexpr auto contains(Vec2 a, Vec2 b, Vec2 c, Vec2 point) noexcept -> bool {
        if (cross(b - a, c - a) == 0.f) {
            return false;
        }
        // An inside point is on the same side of all three edges, or on one.
        const float ab = cross(b - a, point - a);
        const float bc = cross(c - b, point - b);
        const float ca = cross(a - c, point - c);
        const bool any_negative = ab < 0.f || bc < 0.f || ca < 0.f;
        const bool any_positive = ab > 0.f || bc > 0.f || ca > 0.f;
        return !(any_negative && any_positive);
    }

    // The closed polygon through the points, any winding, convex or not. If
    // it crosses itself, parts covered an odd number of times are inside. A
    // point exactly on an edge may give either result. Fewer than three
    // points have no inside.
    [[nodiscard]] constexpr auto contains(std::span<const Vec2> polygon, Vec2 point) noexcept -> bool {
        if (polygon.size() < 3) {
            return false;
        }
        // A ray to the right from an inside point crosses an odd number of
        // edges. A corner at the point's height counts as above it, so a ray
        // through a corner crosses once.
        bool inside = false;
        for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
            const Vec2 a = polygon[i];
            const Vec2 b = polygon[j];
            if ((a.y > point.y) != (b.y > point.y)) {
                const float x = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
                if (point.x < x) {
                    inside = !inside;
                }
            }
        }
        return inside;
    }

    // The point of the rect, edges included, nearest to point: point itself
    // when inside. point minus it is the way out of the rect, e.g. to tell
    // which side a ball hit.
    [[nodiscard]] constexpr auto closest_point(Rect rect, Vec2 point) noexcept -> Vec2 {
        // A negative size would put the clamp bounds in the wrong order.
        TGX_ASSERT_MSG(detail::valid(rect), "rect of size {}x{}", rect.width, rect.height);

        return {std::clamp(point.x, rect.x, rect.right()), std::clamp(point.y, rect.y, rect.bottom())};
    }

    // Touching is not overlapping: two tiles side by side do not overlap.
    [[nodiscard]] constexpr auto overlaps(Rect a, Rect b) noexcept -> bool {
        TGX_ASSERT_MSG(detail::valid(a), "rect of size {}x{}", a.width, a.height);
        TGX_ASSERT_MSG(detail::valid(b), "rect of size {}x{}", b.width, b.height);

        return !a.empty() && !b.empty()
               && a.x < b.right() && b.x < a.right()
               && a.y < b.bottom() && b.y < a.bottom();
    }

    [[nodiscard]] constexpr auto overlaps(Circle a, Circle b) noexcept -> bool {
        TGX_ASSERT_MSG(detail::valid(a), "circle of radius {}", a.radius);
        TGX_ASSERT_MSG(detail::valid(b), "circle of radius {}", b.radius);

        const Vec2 d = b.center - a.center;
        const float reach = a.radius + b.radius;
        return length_squared(d) < reach * reach;
    }

    [[nodiscard]] constexpr auto overlaps(Rect rect, Circle circle) noexcept -> bool {
        // A negative size would put the clamp bounds in the wrong order.
        TGX_ASSERT_MSG(detail::valid(rect), "rect of size {}x{}", rect.width, rect.height);
        TGX_ASSERT_MSG(detail::valid(circle), "circle of radius {}", circle.radius);

        if (rect.empty()) {
            return false;
        }
        const Vec2 d = circle.center - closest_point(rect, circle.center);
        return length_squared(d) < circle.radius * circle.radius;
    }

    [[nodiscard]] constexpr auto overlaps(Circle circle, Rect rect) noexcept -> bool {
        return overlaps(rect, circle);
    }

    // The point on the segment a b nearest to point, e.g. to push a ball
    // away from a wall.
    [[nodiscard]] constexpr auto closest_point(Vec2 a, Vec2 b, Vec2 point) noexcept -> Vec2 {
        const Vec2 ab = b - a;
        const float squared = length_squared(ab);
        if (squared == 0.f) {
            return a;
        }
        const float t = std::clamp(dot(point - a, ab) / squared, 0.f, 1.f);
        return a + ab * t;
    }

    // The circle and the segment a b. Touching is not overlapping. For a
    // point near a line, use a small circle: overlaps(Circle{point, 2}, a, b).
    [[nodiscard]] constexpr auto overlaps(Circle circle, Vec2 a, Vec2 b) noexcept -> bool {
        TGX_ASSERT_MSG(detail::valid(circle), "circle of radius {}", circle.radius);

        const Vec2 d = circle.center - closest_point(a, b, circle.center);
        return length_squared(d) < circle.radius * circle.radius;
    }

    // The overlapping part, or empty. Its size is how deep one is in the
    // other, e.g. how far to push a player out of a wall.
    [[nodiscard]] constexpr auto intersection(Rect a, Rect b) noexcept -> Rect {
        TGX_ASSERT_MSG(detail::valid(a), "rect of size {}x{}", a.width, a.height);
        TGX_ASSERT_MSG(detail::valid(b), "rect of size {}x{}", b.width, b.height);

        const float left = std::max(a.x, b.x);
        const float top = std::max(a.y, b.y);
        const float right = std::min(a.right(), b.right());
        const float bottom = std::min(a.bottom(), b.bottom());
        if (right <= left || bottom <= top) {
            return {};
        }
        return {left, top, right - left, bottom - top};
    }

    // Where the segments a1 a2 and b1 b2 cross, ends included. Parallel
    // segments give none, even on top of each other.
    [[nodiscard]] constexpr auto intersection(Vec2 a1, Vec2 a2, Vec2 b1, Vec2 b2) noexcept -> std::optional<Vec2> {
        const Vec2 a = a2 - a1;
        const Vec2 b = b2 - b1;
        const float denominator = cross(a, b);
        if (denominator == 0.f) {
            return std::nullopt;
        }
        // Fractions along each segment: 0 at its start, 1 at its end.
        const Vec2 start = b1 - a1;
        const float t = cross(start, b) / denominator;
        const float u = cross(start, a) / denominator;
        if (t < 0.f || t > 1.f || u < 0.f || u > 1.f) {
            return std::nullopt;
        }
        return a1 + a * t;
    }
}
