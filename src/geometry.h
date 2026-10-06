#pragma once

#include "tgx/color.h"
#include "tgx/math.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

// The arithmetic under drawing and input, kept apart from GL so the tests
// reach it.
namespace tgx::detail {
    // Segments for a circle: enough that no edge strays more than a quarter
    // pixel from the true circle, at the size it shows in pixels.
    inline constexpr float circle_tolerance = 0.25f;
    inline constexpr std::size_t min_segments = 8;
    inline constexpr std::size_t max_segments = 1024;

    [[nodiscard]] inline auto segments_for(float pixel_radius) noexcept -> std::size_t {
        if (pixel_radius <= circle_tolerance) {
            return min_segments;
        }
        // An edge of a circle split into n strays r * (1 - cos(pi / n)) from it.
        // A huge radius rounds the cosine to 1 and n to infinity, which no
        // cast survives: clamped while still a float.
        const float n = std::numbers::pi_v<float> / std::acos(1.f - circle_tolerance / pixel_radius);
        if (!(n < static_cast<float>(max_segments))) {
            return max_segments;
        }
        return std::max(static_cast<std::size_t>(std::ceil(n)), min_segments);
    }

    // The area a canvas's coordinates span: its own size, else the size of
    // what it covers, its viewport or the whole surface (units).
    [[nodiscard]] inline auto span_of(Size size, Rect viewport, Size units) noexcept -> Size {
        if (!size.empty()) {
            return size;
        }
        if (!viewport.empty()) {
            return {static_cast<int>(std::lround(viewport.width)), static_cast<int>(std::lround(viewport.height))};
        }
        return units;
    }

    // Where two lines of a strip meet at a corner: the points both share on
    // either side, plus along the first line's perpendicular, minus the
    // other way. Not mitered when the corner is too sharp for that, turns
    // straight back, or would reach along a line more than room: the lines
    // then end square at it.
    struct Joint {
        Vec2 plus;
        Vec2 minus;
        bool mitered;
    };

    // How far a mitered corner may reach out from the line's middle, in
    // halves of its thickness: SVG's default miter limit of 4 thicknesses
    // from tip to tip.
    inline constexpr float miter_limit = 4.f;

    // Below this squared length the two lines' perpendiculars cancel out:
    // the path turns straight back, and no miter points anywhere.
    inline constexpr float straight_back = 1e-6f;

    // in and out are the directions of the lines, of length 1.
    [[nodiscard]] inline auto joint(Vec2 at, Vec2 in, Vec2 out, float half, float room) noexcept -> Joint {
        const Vec2 sum = perpendicular(in) + perpendicular(out);
        if (dot(sum, sum) < straight_back) {
            return {at, at, false};
        }
        const Vec2 miter = normalize(sum);
        // Projected onto the line's own side, the miter reaches half out.
        const float reach = half / dot(miter, perpendicular(out));
        if (reach > half * miter_limit) {
            return {at, at, false};
        }
        // The inside point slides back along both lines; past the far end of
        // a short one, its band would twist over itself.
        const Vec2 offset = miter * reach;
        if (std::abs(dot(offset, out)) > room) {
            return {at, at, false};
        }
        return {at + offset, at - offset, true};
    }

    // Whether the colors of a quad's corners (clockwise) blend between them
    // as two triangles split along a to c would: when a and c add up to the
    // same as b and d, channel by channel. Then the diagonal does not show.
    [[nodiscard]] inline auto blends_flat(const std::array<Color, 4> &colors) noexcept -> bool {
        const auto flat = [&](std::uint8_t Color::*channel) noexcept {
            return colors[0].*channel + colors[2].*channel == colors[1].*channel + colors[3].*channel;
        };
        return flat(&Color::r) && flat(&Color::g) && flat(&Color::b) && flat(&Color::a);
    }

    // The four corners' colors mixed evenly, rounded.
    [[nodiscard]] inline auto average(const std::array<Color, 4> &colors) noexcept -> Color {
        const auto mix = [&](std::uint8_t Color::*channel) noexcept {
            const int sum = colors[0].*channel + colors[1].*channel + colors[2].*channel + colors[3].*channel;
            return static_cast<std::uint8_t>((sum + 2) / 4);
        };
        return {mix(&Color::r), mix(&Color::g), mix(&Color::b), mix(&Color::a)};
    }

    // Where the held sides point, of length 1, or 0 when nothing is held or
    // opposite sides cancel out.
    [[nodiscard]] inline auto unit_direction(bool left, bool right, bool up, bool down) noexcept -> Vec2 {
        const Vec2 direction{
            static_cast<float>(right) - static_cast<float>(left),
            static_cast<float>(down) - static_cast<float>(up),
        };
        return direction == Vec2{} ? direction : normalize(direction);
    }
}
