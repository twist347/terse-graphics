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

// Math for drawing and input, apart from GL so tests can use it.
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

    // A canvas's coordinate space: its size if set, else its viewport's, else
    // units.
    [[nodiscard]] inline auto span_of(Size size, Rect viewport, Size units) noexcept -> Size {
        if (!size.empty()) {
            return size;
        }
        if (!viewport.empty()) {
            return {static_cast<int>(std::lround(viewport.width)), static_cast<int>(std::lround(viewport.height))};
        }
        return units;
    }

    // Where two lines of a strip meet: the points both share, plus along the
    // first line's perpendicular and minus the other way.
    struct Joint {
        Vec2 plus;
        Vec2 minus;
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

    // 0 when none is held or opposites cancel.
    [[nodiscard]] inline auto unit_direction(bool left, bool right, bool up, bool down) noexcept -> Vec2 {
        const Vec2 direction{
            static_cast<float>(right) - static_cast<float>(left),
            static_cast<float>(down) - static_cast<float>(up),
        };
        return direction == Vec2{} ? direction : normalize(direction);
    }
}
