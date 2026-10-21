#pragma once

#include "tgx/core/math.hpp"

#include "tgx/gl/draw.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

// Where a canvas lands on the window or render target it draws into: the
// part it covers in pixels, its coordinate space, screen points into it.
namespace tgx::detail {
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

    // content as large as fits in area, keeping its shape, centered: bars on
    // two sides. Empty when either is empty. tgx::fit, into a rect anywhere.
    [[nodiscard]] constexpr auto fit_into(Size content, Rect area) noexcept -> Rect {
        if (content.empty() || area.empty()) {
            return {};
        }
        const float scale = std::min(
            area.width / static_cast<float>(content.width),
            area.height / static_cast<float>(content.height)
        );
        const float w = static_cast<float>(content.width) * scale;
        const float h = static_cast<float>(content.height) * scale;
        return {area.x + (area.width - w) / 2.f, area.y + (area.height - h) / 2.f, w, h};
    }

    // Rounded, within int: lround of a huge float is unspecified, and a
    // viewport may be set that large.
    [[nodiscard]] inline auto round_to_int(float x) noexcept -> int {
        constexpr float limit = 1e9f;
        return static_cast<int>(std::lround(std::clamp(x, -limit, limit)));
    }

    // A canvas's coordinate space: its size if set, else its viewport's, else
    // units.
    [[nodiscard]] inline auto span_of(Size size, std::optional<Rect> viewport, Size units) noexcept -> Size {
        if (!size.empty()) {
            return size;
        }
        if (viewport) {
            return {round_to_int(viewport->width), round_to_int(viewport->height)};
        }
        return units;
    }

    // y down, (0, 0) at the top-left. An empty size (minimized window) would
    // divide by zero; nothing shows anyway.
    [[nodiscard]] constexpr auto projection_for(Size size) noexcept -> Mat4 {
        if (size.empty()) {
            return {};
        }
        return ortho(0.f, static_cast<float>(size.width), static_cast<float>(size.height), 0.f);
    }

    // Part of the surface drawn on, in surface units: what the viewport
    // covers (all of it if none), or a size of its own as large as fits in
    // that, centered.
    [[nodiscard]] inline auto shown(Size size, std::optional<Rect> viewport, const Surface &surface) noexcept -> Rect {
        const Vec2 units = to_vec2(surface.units);
        const Rect covered = viewport.value_or(Rect{0.f, 0.f, units.x, units.y});
        return size.empty() ? covered : fit_into(size, covered);
    }

    // A part of the surface in pixels. All of it while the surface has no
    // size to scale by.
    [[nodiscard]] inline auto pixel_viewport(Rect rect, const Surface &surface) noexcept -> gl::Viewport {
        if (surface.units.empty()) {
            return surface.viewport();
        }

        // Rounds the edges, not the sizes, so canvases side by side meet
        // without a gap or an overlap.
        const float sx = static_cast<float>(surface.pixels.width) / static_cast<float>(surface.units.width);
        const float sy = static_cast<float>(surface.pixels.height) / static_cast<float>(surface.units.height);
        const int left = round_to_int(rect.x * sx);
        const int top = round_to_int(rect.y * sy);
        const int right = round_to_int(rect.right() * sx);
        const int bottom = round_to_int(rect.bottom() * sy);
        return {left, top, right - left, bottom - top};
    }

    // A clip rect in the coordinates of a canvas spanning span, in pixels from
    // the top-left, within the part shown. Edges rounded, as pixel_viewport's.
    // Empty when nothing of it is shown.
    [[nodiscard]] inline auto pixel_clip(Rect clip, Size span, gl::Viewport shown) noexcept -> gl::Viewport {
        if (span.empty()) {
            return {shown.x, shown.y, 0, 0};
        }

        const auto edge = [](float units, float pixels_per_unit, int extent) {
            return round_to_int(std::clamp(units * pixels_per_unit, 0.f, static_cast<float>(extent)));
        };
        const float sx = static_cast<float>(shown.width) / static_cast<float>(span.width);
        const float sy = static_cast<float>(shown.height) / static_cast<float>(span.height);
        const int left = edge(clip.x, sx, shown.width);
        const int top = edge(clip.y, sy, shown.height);
        const int right = std::max(left, edge(clip.right(), sx, shown.width));
        const int bottom = std::max(top, edge(clip.bottom(), sy, shown.height));
        return {shown.x + left, shown.y + top, right - left, bottom - top};
    }

    // canvas = (screen - offset) * scale, per axis. None for a minimized
    // window.
    struct ScreenMapping {
        Vec2 offset{};
        Vec2 scale{};

        [[nodiscard]] auto to_canvas(Vec2 screen) const noexcept -> Vec2 {
            return {(screen.x - offset.x) * scale.x, (screen.y - offset.y) * scale.y};
        }

        [[nodiscard]] auto to_screen(Vec2 canvas) const noexcept -> Vec2 {
            return {canvas.x / scale.x + offset.x, canvas.y / scale.y + offset.y};
        }
    };

    [[nodiscard]] inline auto screen_mapping(
        Size size,
        std::optional<Rect> viewport,
        const Surface &surface
    ) noexcept -> std::optional<ScreenMapping> {
        const Rect area = shown(size, viewport, surface);
        const Size span = span_of(size, viewport, surface.units);
        if (area.empty() || span.empty()) {
            return std::nullopt;
        }
        return ScreenMapping{
            area.position(),
            {static_cast<float>(span.width) / area.width, static_cast<float>(span.height) / area.height},
        };
    }
}
