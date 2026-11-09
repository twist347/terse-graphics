#pragma once

#include "tgx/core/color.hpp"
#include "tgx/core/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// Math for drawing, apart from GL so tests can use it.
namespace tgx::detail {
    // Segments for a circle: enough that no edge is more than a quarter pixel
    // off the true circle.
    inline constexpr float circle_tolerance = 0.25f;
    inline constexpr std::size_t min_segments = 8;
    inline constexpr std::size_t max_segments = 1024;

    [[nodiscard]] inline auto segments_for(float pixel_radius) noexcept -> std::size_t {
        if (pixel_radius <= circle_tolerance) {
            return min_segments;
        }
        // An edge is up to r * (1 - cos(pi / n)) off. A huge radius rounds
        // the cosine to 1 and n to infinity, which breaks an integer cast, so
        // n is clamped while still a float.
        const float n = std::numbers::pi_v<float> / std::acos(1.f - circle_tolerance / pixel_radius);
        if (!(n < static_cast<float>(max_segments))) {
            return max_segments;
        }
        return std::max(static_cast<std::size_t>(std::ceil(n)), min_segments);
    }

    // Whether normalize(v) is a direction: its squared length neither
    // underflows to 0 (a line of 1e-30) nor overflows to infinity.
    [[nodiscard]] inline auto has_direction(Vec2 v) noexcept -> bool {
        const float squared = length_squared(v);
        return squared > 0.f && std::isfinite(squared);
    }

    // Ear clipping of a simple polygon. Returns indices into points. Adjacent
    // duplicates, a repeated closing point and straight-through vertices are
    // skipped; either winding works.
    [[nodiscard]] inline auto triangulate_polygon(
        std::span<const Vec2> points
    ) -> std::vector<std::array<std::size_t, 3>> {
        std::vector<std::array<std::size_t, 3>> triangles;
        if (points.size() < 3) {
            return triangles;
        }

        std::vector<std::size_t> remaining;
        remaining.reserve(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (i == 0 || points[i] != points[i - 1]) {
                remaining.push_back(i);
            }
        }
        if (remaining.size() > 1 && points[remaining.front()] == points[remaining.back()]) {
            remaining.pop_back();
        }
        while (remaining.size() >= 3) {
            bool removed = false;
            for (std::size_t i = 0; i < remaining.size(); ++i) {
                const Vec2 a = points[remaining[(i + remaining.size() - 1) % remaining.size()]];
                const Vec2 b = points[remaining[i]];
                const Vec2 c = points[remaining[(i + 1) % remaining.size()]];
                if (cross(b - a, c - b) == 0.f && dot(b - a, c - b) >= 0.f) {
                    remaining.erase(remaining.begin() + i);
                    removed = true;
                    break;
                }
            }
            if (!removed) {
                break;
            }
        }
        if (remaining.size() < 3) {
            return triangles;
        }
        triangles.reserve(remaining.size() - 2);

        const Vec2 origin = points[remaining.front()];
        float area = 0.f;
        for (std::size_t i = 0; i < remaining.size(); ++i) {
            const Vec2 a = points[remaining[i]] - origin;
            const Vec2 b = points[remaining[(i + 1) % remaining.size()]] - origin;
            area += cross(a, b);
        }
        if (area == 0.f) {
            return triangles;
        }
        const float winding = area > 0.f ? 1.f : -1.f;

        while (remaining.size() > 3) {
            bool clipped = false;
            for (std::size_t i = 0; i < remaining.size(); ++i) {
                const std::size_t previous = remaining[(i + remaining.size() - 1) % remaining.size()];
                const std::size_t current = remaining[i];
                const std::size_t next = remaining[(i + 1) % remaining.size()];
                const Vec2 a = points[previous];
                const Vec2 b = points[current];
                const Vec2 c = points[next];
                if (cross(b - a, c - b) * winding <= 0.f) {
                    continue;
                }

                bool blocked = false;
                for (const std::size_t other: remaining) {
                    if (other == previous || other == current || other == next) {
                        continue;
                    }
                    const Vec2 p = points[other];
                    const bool inside = cross(b - a, p - a) * winding >= 0.f
                                        && cross(c - b, p - b) * winding >= 0.f
                                        && cross(a - c, p - c) * winding >= 0.f;
                    if (inside) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) {
                    continue;
                }

                triangles.push_back({previous, current, next});
                remaining.erase(remaining.begin() + i);
                clipped = true;
                break;
            }
            if (clipped) {
                continue;
            }

            bool removed = false;
            for (std::size_t i = 0; i < remaining.size(); ++i) {
                const Vec2 a = points[remaining[(i + remaining.size() - 1) % remaining.size()]];
                const Vec2 b = points[remaining[i]];
                const Vec2 c = points[remaining[(i + 1) % remaining.size()]];
                if (cross(b - a, c - b) == 0.f && dot(b - a, c - b) >= 0.f) {
                    remaining.erase(remaining.begin() + i);
                    removed = true;
                    break;
                }
            }
            if (!removed) {
                break;
            }
        }

        if (remaining.size() == 3) {
            const Vec2 a = points[remaining[0]];
            const Vec2 b = points[remaining[1]];
            const Vec2 c = points[remaining[2]];
            if (cross(b - a, c - b) * winding > 0.f) {
                triangles.push_back({remaining[0], remaining[1], remaining[2]});
            }
        }
        return triangles;
    }

    // Radians.
    inline constexpr float whole_turn = 2.f * std::numbers::pi_v<float>;

    // Start angle and clockwise sweep, at most a full turn.
    struct Arc {
        float start{};
        float sweep{};
    };

    [[nodiscard]] inline auto arc_between(float start, float end) noexcept -> Arc {
        if (end < start) {
            std::swap(start, end);
        }
        return {.start = start, .sweep = std::min(end - start, whole_turn)};
    }

    // Segments for the arc, given n for the whole circle. At least one.
    [[nodiscard]] inline auto arc_segments(const Arc &arc, std::size_t n) noexcept -> std::size_t {
        const float part = arc.sweep / whole_turn;
        return std::max(static_cast<std::size_t>(std::ceil(part * static_cast<float>(n))), std::size_t{1});
    }

    // A full circle's last point is its first; a closed fan or band reuses
    // it exactly, leaving no seam.
    [[nodiscard]] inline auto full_turn(const Arc &arc) noexcept -> bool {
        return arc.sweep >= whole_turn;
    }

    // As circle() draws it.
    inline constexpr Arc full_circle{.start = 0.f, .sweep = whole_turn};

    // Point i of a unit-circle arc split into n segments.
    [[nodiscard]] inline auto arc_point(const Arc &arc, std::size_t i, std::size_t n) noexcept -> Vec2 {
        return from_angle(arc.start + arc.sweep * static_cast<float>(i) / static_cast<float>(n));
    }

    // The points of an arc split into n segments, radius around center, as
    // push_fan and push_band take them.
    struct Rim {
        Vec2 center{};
        float radius{};
        Arc arc{};
        std::size_t n{};

        [[nodiscard]] auto operator()(std::size_t i) const noexcept -> Vec2 {
            return center + arc_point(arc, i, n) * radius;
        }
    };

    // One axis of an EllipseRim point moved inset along the normal, kept on
    // its own side and within radius - inset: where the edge bends tighter
    // than inset, the moved points would cross over.
    [[nodiscard]] inline auto inward(float at, float normal, float radius, float inset) noexcept -> float {
        const float limit = radius - inset;
        const float moved = at - normal * inset;
        return at < 0.f ? std::clamp(moved, -limit, 0.f) : std::clamp(moved, 0.f, limit);
    }

    // As Rim, for a whole ellipse turned by an angle of cosine and sine turn.
    // inset moves the points inward, for an outline; under min(radii).
    struct EllipseRim {
        Vec2 center{};
        Vec2 radii{};
        Vec2 turn{};
        std::size_t n{};
        float inset{0.f};

        [[nodiscard]] auto operator()(std::size_t i) const noexcept -> Vec2 {
            const Vec2 unit = arc_point(full_circle, i, n);
            Vec2 at{unit.x * radii.x, unit.y * radii.y};
            if (inset > 0.f) {
                const Vec2 normal = normalize(Vec2{unit.x / radii.x, unit.y / radii.y});
                at = {inward(at.x, normal.x, radii.x, inset), inward(at.y, normal.y, radii.y, inset)};
            }
            return center + turned(at, turn);
        }
    };

    // Clockwise from the top-left, as push_quad takes them.
    [[nodiscard]] constexpr auto corners(
        Vec2 top_left,
        Vec2 bottom_right
    ) noexcept -> std::array<Vec2, 4> {
        return {top_left, {bottom_right.x, top_left.y}, bottom_right, {top_left.x, bottom_right.y}};
    }

    [[nodiscard]] constexpr auto corners(Rect rect) noexcept -> std::array<Vec2, 4> {
        return corners(rect.position(), {rect.right(), rect.bottom()});
    }

    // from_angle, without cos and sin for the usual unturned shape.
    [[nodiscard]] inline auto turn_of(float rotation) noexcept -> Vec2 {
        return rotation == 0.f ? Vec2{1.f, 0.f} : from_angle(rotation);
    }

    // A point of a box, from its top-left corner, where the box goes: origin
    // (also from the top-left) at position, turned around it as Sprite is.
    // turn is turn_of(rotation).
    [[nodiscard]] constexpr auto placed(Vec2 point, Vec2 origin, Vec2 position, Vec2 turn) noexcept -> Vec2 {
        return turned(point - origin, turn) + position;
    }

    // Clockwise from the top-left, placed as placed() does.
    [[nodiscard]] inline auto placed_corners(
        Vec2 size,
        Vec2 origin,
        Vec2 position,
        float rotation
    ) noexcept -> std::array<Vec2, 4> {
        const Vec2 turn = turn_of(rotation);
        auto box = corners({}, size);
        for (Vec2 &corner: box) {
            corner = placed(corner, origin, position, turn);
        }
        return box;
    }

    // Point i on the edge of a rounded rectangle, clockwise from the left end
    // of the top-left corner. inset moves the edge inward, for an outline:
    // corners keep their centers, and are square once inset reaches radius.
    [[nodiscard]] inline auto rounded_rim(
        Rect rect,
        float radius,
        float inset,
        std::size_t points,
        std::size_t i
    ) noexcept -> Vec2 {
        const std::size_t corner = i / points;
        const std::size_t step = i % points;
        const float in = std::max(radius, inset);
        const auto centers = corners({rect.x + in, rect.y + in}, {rect.right() - in, rect.bottom() - in});
        // The top-left quarter starts at half a turn (left); each next corner
        // a quarter turn later.
        constexpr float quarter = whole_turn / 4.f;
        const Arc arc{.start = quarter * static_cast<float>(corner + 2), .sweep = quarter};
        return centers[corner] + arc_point(arc, step, points - 1) * std::max(radius - inset, 0.f);
    }

    // Per rounded corner, given n segments for the whole circle.
    [[nodiscard]] inline auto corner_points(std::size_t n) noexcept -> std::size_t {
        return arc_segments({0.f, whole_turn / 4.f}, n) + 1;
    }

    // Where two lines of a strip meet: the points both share, plus along the
    // first line's perpendicular and minus the other way.
    struct Joint {
        Vec2 plus{};
        Vec2 minus{};
    };

    // How far a miter may reach from the line's center, in half thicknesses.
    // Matches SVG's default miter limit of 4.
    inline constexpr float miter_limit = 4.f;

    // Below this squared length the perpendiculars cancel: the path turns
    // straight back and the miter has no direction.
    inline constexpr float straight_back = 1e-6f;

    // The mitered joint of lines with unit directions in and out, or none
    // when too sharp, when the path turns straight back, or when the miter
    // would reach along a line further than room: the lines then end square.
    [[nodiscard]] inline auto joint(
        Vec2 at,
        Vec2 in,
        Vec2 out,
        float half,
        float room
    ) noexcept -> std::optional<Joint> {
        const Vec2 sum = perpendicular(in) + perpendicular(out);
        if (length_squared(sum) < straight_back) {
            return std::nullopt;
        }
        const Vec2 miter = normalize(sum);
        // Projected onto the perpendicular, the miter must reach half the
        // thickness.
        const float reach = half / dot(miter, perpendicular(out));
        if (reach > half * miter_limit) {
            return std::nullopt;
        }
        // The inner point moves back along both lines; past the far end of a
        // short line, that line would twist.
        const Vec2 offset = miter * reach;
        if (std::abs(dot(offset, out)) > room) {
            return std::nullopt;
        }
        return Joint{at + offset, at - offset};
    }

    // Whether the diagonal of a quad (corners clockwise) would not show: a + c
    // equals b + d in every channel.
    [[nodiscard]] inline auto blends_flat(const std::array<Color, 4> &colors) noexcept -> bool {
        const auto flat = [&](std::uint8_t Color::*channel) noexcept {
            return colors[0].*channel + colors[2].*channel == colors[1].*channel + colors[3].*channel;
        };
        return flat(&Color::r) && flat(&Color::g) && flat(&Color::b) && flat(&Color::a);
    }

    // Rounded.
    [[nodiscard]] inline auto average(const std::array<Color, 4> &colors) noexcept -> Color {
        const auto mix = [&](std::uint8_t Color::*channel) noexcept {
            const int sum = colors[0].*channel + colors[1].*channel + colors[2].*channel + colors[3].*channel;
            return static_cast<std::uint8_t>((sum + 2) / 4);
        };
        return {mix(&Color::r), mix(&Color::g), mix(&Color::b), mix(&Color::a)};
    }
}
