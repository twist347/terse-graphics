#include "tgx/core/canvas.hpp"

#include "tgx/core/assert.hpp"
#include "tgx/core/collision.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/texture.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/shader.hpp"

#include "core/batch.hpp"
#include "core/context.hpp"
#include "core/default_font.hpp"
#include "core/geometry.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <numbers>
#include <optional>
#include <string_view>
#include <utility>

namespace {
    using tgx::detail::Batch;
    using tgx::detail::BatchState;
    using tgx::detail::Surface;

    static_assert(
        tgx::detail::max_segments * 2 <= tgx::detail::batch_max_vertices
        && tgx::detail::max_segments * 6 <= tgx::detail::batch_max_indices
    );

    namespace font = tgx::detail::default_font;

    // Shapes sample the white block's center; its neighbors are white too.
    constexpr tgx::Vec2 white_uv{
        (static_cast<float>(font::white_x) + font::white_size / 2.f) / font::atlas_width,
        (font::white_size / 2.f) / font::atlas_height,
    };

    // Glyphs are one row filling the atlas height, so v goes from 0 to 1.
    static_assert(font::atlas_height == font::line_height);

    // Glyph index for a line break.
    constexpr std::size_t newline = font::glyphs.size();

    // Moves the pen, draws nothing.
    constexpr std::size_t space = static_cast<std::size_t>(' ' - font::first);

    // Leading one bits. 1 for ASCII, a stray continuation or an invalid lead.
    [[nodiscard]] constexpr auto sequence_length(unsigned char lead) noexcept -> std::size_t {
        const int ones = std::countl_one(lead);
        return ones >= 2 && ones <= 4 ? static_cast<std::size_t>(ones) : 1;
    }

    // Columns between tab stops.
    constexpr std::size_t tab_columns = 4;

    // Calls f with each character's glyph index, or newline. One UTF-8
    // sequence is one character, '?' without a glyph. '\t' pads to the next
    // tab stop (monospaced font). '\r' is skipped, so "\r\n" ends a line once.
    template<typename F>
    auto for_each_glyph(std::string_view text, F f) noexcept -> void {
        // Since the start of the line.
        std::size_t column = 0;
        for (std::size_t i = 0; i < text.size();) {
            const auto c = static_cast<unsigned char>(text[i]);
            // Only continuation bytes are taken, so a cut-off sequence does
            // not swallow the next character.
            const std::size_t length = sequence_length(c);
            ++i;
            for (std::size_t taken = 1; taken < length && i < text.size(); ++taken, ++i) {
                if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) {
                    break;
                }
            }

            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                f(newline);
                column = 0;
                continue;
            }
            if (c == '\t') {
                do {
                    f(space);
                    ++column;
                } while (column % tab_columns != 0);
                continue;
            }
            const bool printable = c >= static_cast<unsigned char>(font::first)
                && c <= static_cast<unsigned char>(font::last);
            f(static_cast<std::size_t>((printable ? static_cast<char>(c) : '?') - font::first));
            ++column;
        }
    }

    // y down, (0, 0) at the top-left. An empty size (minimized window) would
    // divide by zero; nothing shows anyway.
    [[nodiscard]] constexpr auto projection_for(tgx::Size size) noexcept -> tgx::Mat4 {
        if (size.empty()) {
            return {};
        }
        return tgx::ortho(0.f, static_cast<float>(size.width), static_cast<float>(size.height), 0.f);
    }

    // Part of the surface covered, in surface units.
    [[nodiscard]] auto covered(tgx::Rect viewport, const Surface &surface) noexcept -> tgx::Rect {
        if (!viewport.empty()) {
            return viewport;
        }
        return {0.f, 0.f, static_cast<float>(surface.units.width), static_cast<float>(surface.units.height)};
    }

    // Part of the surface drawn on, in surface units: a size of its own as
    // large as fits in what is covered, centered; else the viewport as set.
    [[nodiscard]] auto shown(tgx::Size size, tgx::Rect viewport, const Surface &surface) noexcept -> tgx::Rect {
        if (size.empty()) {
            return viewport;
        }
        return tgx::detail::fit_into(size, covered(viewport, surface));
    }

    // Part of the surface covered, in pixels.
    [[nodiscard]] auto pixel_viewport(tgx::Rect rect, const Surface &surface) noexcept -> tgx::gl::Viewport {
        if (rect.empty() || surface.units.empty()) {
            return surface.viewport();
        }

        // Rounds the edges, not the sizes, so canvases side by side meet
        // without a gap or an overlap.
        const float sx = static_cast<float>(surface.pixels.width) / static_cast<float>(surface.units.width);
        const float sy = static_cast<float>(surface.pixels.height) / static_cast<float>(surface.units.height);
        const auto left = static_cast<int>(std::lround(rect.x * sx));
        const auto top = static_cast<int>(std::lround(rect.y * sy));
        const auto right = static_cast<int>(std::lround(rect.right() * sx));
        const auto bottom = static_cast<int>(std::lround(rect.bottom() * sy));
        return {left, top, right - left, bottom - top};
    }

    // canvas = (screen - offset) * scale, per axis. None for a minimized
    // window.
    struct ScreenMapping {
        tgx::Vec2 offset{};
        tgx::Vec2 scale{};

        [[nodiscard]] auto to_canvas(tgx::Vec2 screen) const noexcept -> tgx::Vec2 {
            return {(screen.x - offset.x) * scale.x, (screen.y - offset.y) * scale.y};
        }

        [[nodiscard]] auto to_screen(tgx::Vec2 canvas) const noexcept -> tgx::Vec2 {
            return {canvas.x / scale.x + offset.x, canvas.y / scale.y + offset.y};
        }
    };

    [[nodiscard]] auto screen_mapping(
        tgx::Size size,
        tgx::Rect viewport,
        const Surface &surface
    ) noexcept -> std::optional<ScreenMapping> {
        const tgx::Rect area = covered(shown(size, viewport, surface), surface);
        const tgx::Size span = tgx::detail::span_of(size, viewport, surface.units);
        if (area.empty() || span.empty()) {
            return std::nullopt;
        }
        return ScreenMapping{
            {area.x, area.y},
            {static_cast<float>(span.width) / area.width, static_cast<float>(span.height) / area.height},
        };
    }

    // The Device's batch, with room for this many more vertices and indices
    // with the state, and the index of the first new vertex.
    [[nodiscard]] auto start(
        const BatchState &state,
        std::size_t vertex_count,
        std::size_t index_count
    ) noexcept -> std::pair<Batch &, std::uint16_t> {
        tgx::detail::Context &context = tgx::detail::context();
        Batch &batch = *context.batch;
        const std::uint16_t first = batch.reserve(context, state, vertex_count, index_count);
        return {batch, first};
    }

    auto push_indices(Batch &batch, std::uint16_t first, std::initializer_list<std::size_t> offsets) noexcept -> void {
        for (const std::size_t offset: offsets) {
            batch.push_index(static_cast<std::uint16_t>(first + offset));
        }
    }

    // Corners clockwise, drawn as triangles a b c and c d a.
    auto push_quad(
        const BatchState &state,
        const std::array<tgx::Vec2, 4> &corners,
        const std::array<tgx::Vec2, 4> &uvs,
        const std::array<tgx::Color, 4> &colors
    ) noexcept -> void {
        auto [batch, first] = start(state, 4, 6);
        for (std::size_t i = 0; i < 4; ++i) {
            batch.push_vertex({corners[i], uvs[i], colors[i]});
        }
        push_indices(batch, first, {0, 1, 2, 2, 3, 0});
    }

    // A triangle from the center to each segment of the rim. closed: the
    // last segment ends at point(0); open: at point(segments).
    template<typename Point>
    auto push_fan(
        const BatchState &state,
        tgx::Vec2 center,
        std::size_t segments,
        Point point,
        tgx::Color inner, tgx::Color outer,
        bool closed
    ) noexcept -> void {
        TGX_ASSERT(segments >= 1);

        const std::size_t count = closed ? segments : segments + 1;
        auto [batch, first] = start(state, count + 1, segments * 3);
        batch.push_vertex({center, white_uv, inner});
        for (std::size_t i = 0; i < count; ++i) {
            batch.push_vertex({point(i), white_uv, outer});
        }
        for (std::size_t i = 0; i < segments; ++i) {
            push_indices(batch, first, {0, 1 + i, 1 + (i + 1) % count});
        }
    }

    // An outline between outer(i) and inner(i), a quad per segment. closed:
    // the last segment ends at pair 0; open: at pair segments.
    template<typename Outer, typename Inner>
    auto push_band(
        const BatchState &state,
        std::size_t segments,
        Outer outer, Inner inner,
        tgx::Color color,
        bool closed
    ) noexcept -> void {
        TGX_ASSERT(segments >= 1);

        const std::size_t count = closed ? segments : segments + 1;
        auto [batch, first] = start(state, count * 2, segments * 6);

        // Outer and inner points in pairs; each pair and the next make a quad.
        for (std::size_t i = 0; i < count; ++i) {
            batch.push_vertex({outer(i), white_uv, color});
            batch.push_vertex({inner(i), white_uv, color});
        }
        for (std::size_t i = 0; i < segments; ++i) {
            const std::size_t here = 2 * i;
            const std::size_t next = 2 * ((i + 1) % count);
            push_indices(batch, first, {here, next, next + 1, next + 1, here + 1, here});
        }
    }

    // Outline, drawn inside the edges. The polygon needs a circle touching
    // every edge (center, inradius), like a triangle or a regular polygon:
    // scaling the corners toward it moves every edge thickness inward.
    // Asserts thickness < inradius.
    template<typename Corner>
    auto push_outline(
        const BatchState &state,
        std::size_t count,
        Corner corner,
        tgx::Vec2 center,
        float inradius,
        float thickness,
        tgx::Color color
    ) noexcept -> void {
        TGX_ASSERT(thickness < inradius);

        const float keep = 1.f - thickness / inradius;
        const auto inner = [&](std::size_t i) noexcept { return center + (corner(i) - center) * keep; };
        push_band(state, count, corner, inner, color, true);
    }

    // Asserted: at least 3, and no more than fit in one draw.
    [[nodiscard]] auto poly_sides(int sides) noexcept -> std::size_t {
        TGX_ASSERT_MSG(
            sides >= 3 && static_cast<std::size_t>(sides) <= tgx::detail::max_segments,
            "polygon of {} sides", sides
        );
        return static_cast<std::size_t>(std::clamp(sides, 3, static_cast<int>(tgx::detail::max_segments)));
    }
}

namespace tgx {
    auto Canvas::create(Size size) noexcept -> Canvas {
        return Canvas{size};
    }

    auto Canvas::set_size(Size size) noexcept -> void {
        // A negative size would silently count as empty.
        TGX_ASSERT_MSG(size.width >= 0 && size.height >= 0, "canvas of size {}x{}", size.width, size.height);

        size_ = size;
        transform_stale_ = true;
    }

    auto Canvas::size() const noexcept -> Size {
        return detail::span_of(size_, viewport_, surface().units);
    }

    auto Canvas::set_viewport(Rect rect) noexcept -> void {
        // A negative size would silently count as empty.
        TGX_ASSERT_MSG(
            std::isfinite(rect.x) && std::isfinite(rect.y) && rect.width >= 0.f && rect.height >= 0.f
                && std::isfinite(rect.width) && std::isfinite(rect.height),
            "viewport {} {} {}x{}",
            rect.x, rect.y, rect.width, rect.height
        );

        viewport_ = rect;
        transform_stale_ = true;
    }

    auto Canvas::set_target(const RenderTarget *target) noexcept -> void {
        target_ = detail::target_of(target);
        transform_stale_ = true;
    }

    auto Canvas::set_camera(const Camera2D &camera) noexcept -> void {
        TGX_ASSERT_MSG(camera.zoom != 0.f, "a camera with zoom 0 shows nothing and cannot map back");
        TGX_ASSERT_MSG(
            std::isfinite(camera.zoom) && std::isfinite(camera.rotation),
            "a camera with zoom {} and rotation {}",
            camera.zoom, camera.rotation
        );

        camera_ = camera;
        transform_stale_ = true;
    }

    auto Canvas::to_world(Vec2 point) const noexcept -> Vec2 {
        const auto mapping = screen_mapping(size_, viewport_, surface());
        return camera_.to_world(mapping ? mapping->to_canvas(point) : point);
    }

    auto Canvas::to_screen(Vec2 world) const noexcept -> Vec2 {
        const auto mapping = screen_mapping(size_, viewport_, surface());
        const Vec2 point = camera_.to_screen(world);
        return mapping ? mapping->to_screen(point) : point;
    }

    auto Canvas::set_shader(const gl::Shader *shader) noexcept -> void {
        program_ = shader ? shader->id() : 0;
        u_projection_ = -1;
        u_texture_ = -1;
        if (!shader) {
            return;
        }

        // The only required uniform; the lookup asserts it is a mat4.
        u_projection_ = gl::detail::location(shader->uniform<Mat4>("u_projection"));

        // Optional. The batch skips Device::draw's checks, so samplers are
        // checked here.
        const auto samplers = gl::detail::samplers(*shader);
        for (const auto &sampler: samplers) {
            TGX_ASSERT_MSG(
                sampler.name == "u_texture",
                "sampler '{}': a canvas shader gets no texture but u_texture",
                sampler.name
            );
        }
        if (!samplers.empty()) {
            u_texture_ = gl::detail::location(shader->uniform<gl::TextureSlot>("u_texture"));
        }
    }

    auto Canvas::clear(Color color) noexcept -> void {
        detail::context().clear({.target = target_, .color = color});
    }

    auto Canvas::rect(Rect rect, Color color) noexcept -> void {
        quad(detail::corners(rect), {color, color, color, color});
    }

    auto Canvas::rect(Rect rect, Vec2 origin, float rotation, Color color) noexcept -> void {
        const auto box = detail::placed_corners({rect.width, rect.height}, origin, {rect.x, rect.y}, rotation);
        quad(box, {color, color, color, color});
    }

    auto Canvas::rect_lines(Rect rect, Color color, float thickness) noexcept -> void {
        TGX_ASSERT_MSG(thickness >= 0.f, "thickness {}", thickness);

        // Thick enough, the outline fills the rectangle.
        const float t = std::min({thickness, rect.width / 2.f, rect.height / 2.f});
        if (t <= 0.f) {
            return;
        }

        // No corner is drawn twice, so a transparent outline stays even.
        this->rect({rect.x, rect.y, rect.width, t}, color);
        this->rect({rect.x, rect.bottom() - t, rect.width, t}, color);
        this->rect({rect.x, rect.y + t, t, rect.height - 2.f * t}, color);
        this->rect({rect.right() - t, rect.y + t, t, rect.height - 2.f * t}, color);
    }

    auto Canvas::rect_rounded(Rect rect, float radius, Color color) noexcept -> void {
        TGX_ASSERT_MSG(detail::valid_radius(radius), "corners of radius {}", radius);

        const float r = std::min({radius, rect.width / 2.f, rect.height / 2.f});
        if (r <= 0.f) {
            this->rect(rect, color);
            return;
        }

        const BatchState state = state_for(0);
        const std::size_t points = detail::corner_points(segments(r, state));
        const auto rim = [&](std::size_t i) noexcept { return detail::rounded_rim(rect, r, 0.f, points, i); };
        push_fan(state, rect.center(), points * 4, rim, color, color, true);
    }

    auto Canvas::rect_rounded_lines(Rect rect, float radius, Color color, float thickness) noexcept -> void {
        TGX_ASSERT_MSG(detail::valid_radius(radius), "corners of radius {}", radius);
        TGX_ASSERT_MSG(thickness >= 0.f, "thickness {}", thickness);

        const float r = std::min({radius, rect.width / 2.f, rect.height / 2.f});
        if (r <= 0.f) {
            rect_lines(rect, color, thickness);
            return;
        }
        if (thickness <= 0.f) {
            return;
        }
        // Thick enough, the outline fills the rectangle.
        if (thickness >= std::min(rect.width, rect.height) / 2.f) {
            rect_rounded(rect, r, color);
            return;
        }

        const BatchState state = state_for(0);
        const std::size_t points = detail::corner_points(segments(r, state));
        push_band(
            state,
            points * 4,
            [&](std::size_t i) noexcept { return detail::rounded_rim(rect, r, 0.f, points, i); },
            [&](std::size_t i) noexcept { return detail::rounded_rim(rect, r, thickness, points, i); },
            color,
            true
        );
    }

    auto Canvas::rect_gradient(Rect rect, Color top, Color bottom) noexcept -> void {
        rect_gradient(rect, top, top, bottom, bottom);
    }

    auto Canvas::rect_gradient(
        Rect rect,
        Color top_left, Color top_right,
        Color bottom_right, Color bottom_left
    ) noexcept -> void {
        const auto box = detail::corners(rect);
        const std::array colors{top_left, top_right, bottom_right, bottom_left};
        if (detail::blends_flat(colors)) {
            quad(box, colors);
            return;
        }

        // Four triangles around the center: a single diagonal would show as
        // a band when opposite corners differ.
        auto [batch, first] = start(state_for(0), 5, 12);
        batch.push_vertex({rect.center(), white_uv, detail::average(colors)});
        for (std::size_t i = 0; i < 4; ++i) {
            batch.push_vertex({box[i], white_uv, colors[i]});
        }
        push_indices(batch, first, {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1});
    }

    auto Canvas::triangle(Vec2 a, Vec2 b, Vec2 c, Color color) noexcept -> void {
        triangle_gradient(a, b, c, color, color, color);
    }

    auto Canvas::triangle_gradient(
        Vec2 a, Vec2 b, Vec2 c,
        Color color_a, Color color_b, Color color_c
    ) noexcept -> void {
        auto [batch, first] = start(state_for(0), 3, 3);

        batch.push_vertex({a, white_uv, color_a});
        batch.push_vertex({b, white_uv, color_b});
        batch.push_vertex({c, white_uv, color_c});
        push_indices(batch, first, {0, 1, 2});
    }

    auto Canvas::triangle_lines(Vec2 a, Vec2 b, Vec2 c, Color color, float thickness) noexcept -> void {
        TGX_ASSERT_MSG(thickness >= 0.f, "thickness {}", thickness);

        const float ab = length(b - a);
        const float bc = length(c - b);
        const float ca = length(a - c);
        const float perimeter = ab + bc + ca;
        if (perimeter == 0.f || thickness <= 0.f) {
            return;
        }

        // Thick enough, the outline fills the triangle.
        const float inradius = std::abs(cross(b - a, c - a)) / perimeter;
        if (thickness >= inradius) {
            triangle(a, b, c, color);
            return;
        }
        const Vec2 incenter = (a * bc + b * ca + c * ab) / perimeter;

        const std::array corners{a, b, c};
        const auto corner = [&](std::size_t i) noexcept { return corners[i]; };
        push_outline(state_for(0), corners.size(), corner, incenter, inradius, thickness, color);
    }

    auto Canvas::line(Vec2 a, Vec2 b, Color color, float thickness) noexcept -> void {
        TGX_ASSERT_MSG(thickness >= 0.f, "thickness {}", thickness);

        // A zero-length line has no direction to widen across.
        const Vec2 along = b - a;
        if (along == Vec2{} || thickness <= 0.f) {
            return;
        }

        const Vec2 side = perpendicular(normalize(along)) * (thickness / 2.f);
        quad(a + side, b + side, b - side, a - side, color);
    }

    auto Canvas::line_strip(std::span<const Vec2> points, Color color, float thickness) noexcept -> void {
        strip(points, color, thickness, false);
    }

    auto Canvas::line_loop(std::span<const Vec2> points, Color color, float thickness) noexcept -> void {
        strip(points, color, thickness, true);
    }

    auto Canvas::circle(Vec2 center, float radius, Color color) noexcept -> void {
        circle_gradient(center, radius, color, color);
    }

    auto Canvas::circle_gradient(Vec2 center, float radius, Color inner, Color outer) noexcept -> void {
        // Also catches NaN, which would give any number of segments.
        TGX_ASSERT_MSG(detail::valid_radius(radius), "circle of radius {}", radius);

        if (radius == 0.f) {
            return;
        }

        const BatchState state = state_for(0);
        const std::size_t n = segments(radius, state);
        push_fan(state, center, n, detail::Rim{center, radius, detail::full_circle, n}, inner, outer, true);
    }

    auto Canvas::circle_lines(Vec2 center, float radius, Color color, float thickness) noexcept -> void {
        TGX_ASSERT_MSG(detail::valid_radius(radius), "circle of radius {}", radius);
        TGX_ASSERT_MSG(thickness >= 0.f, "thickness {}", thickness);

        // Thick enough, the ring has no hole.
        constexpr detail::Arc arc = detail::full_circle;
        ring(center, std::max(radius - thickness, 0.f), radius, arc.start, arc.sweep, color);
    }

    auto Canvas::circle_sector(Vec2 center, float radius, float start, float end, Color color) noexcept -> void {
        TGX_ASSERT_MSG(detail::valid_radius(radius), "circle of radius {}", radius);
        TGX_ASSERT_MSG(std::isfinite(start) && std::isfinite(end), "sector from {} to {}", start, end);

        const detail::Arc arc = detail::arc_between(start, end);
        if (radius == 0.f || arc.sweep == 0.f) {
            return;
        }

        const BatchState state = state_for(0);
        const std::size_t n = detail::arc_segments(arc, segments(radius, state));
        const bool full = detail::full_turn(arc);
        push_fan(state, center, n, detail::Rim{center, radius, arc, n}, color, color, full);
    }

    auto Canvas::ring(
        Vec2 center,
        float inner_radius, float outer_radius,
        float start, float end,
        Color color
    ) noexcept -> void {
        TGX_ASSERT_MSG(
            detail::valid_radius(inner_radius) && detail::valid_radius(outer_radius) && inner_radius <= outer_radius,
            "ring from radius {} to {}",
            inner_radius, outer_radius
        );
        TGX_ASSERT_MSG(std::isfinite(start) && std::isfinite(end), "ring from {} to {}", start, end);

        const detail::Arc arc = detail::arc_between(start, end);
        if (inner_radius == outer_radius || arc.sweep == 0.f) {
            return;
        }

        const BatchState state = state_for(0);
        const std::size_t n = detail::arc_segments(arc, segments(outer_radius, state));
        const bool full = detail::full_turn(arc);
        push_band(
            state,
            n,
            detail::Rim{center, outer_radius, arc, n},
            detail::Rim{center, inner_radius, arc, n},
            color,
            full
        );
    }

    auto Canvas::ellipse(Vec2 center, Vec2 radii, Color color, float rotation) noexcept -> void {
        TGX_ASSERT_MSG(
            detail::valid_radius(radii.x) && detail::valid_radius(radii.y),
            "ellipse of radii {} {}", radii.x, radii.y
        );
        TGX_ASSERT_MSG(std::isfinite(rotation), "rotation {}", rotation);

        if (radii.x == 0.f || radii.y == 0.f) {
            return;
        }

        // The chords bend away from the edge no more than on a circle of the
        // larger radius.
        const BatchState state = state_for(0);
        const std::size_t n = segments(std::max(radii.x, radii.y), state);
        const detail::EllipseRim rim{center, radii, from_angle(rotation), n};
        push_fan(state, center, n, rim, color, color, true);
    }

    auto Canvas::ellipse_lines(Vec2 center, Vec2 radii, Color color, float thickness, float rotation) noexcept -> void {
        TGX_ASSERT_MSG(
            detail::valid_radius(radii.x) && detail::valid_radius(radii.y),
            "ellipse of radii {} {}", radii.x, radii.y
        );
        TGX_ASSERT_MSG(thickness >= 0.f, "thickness {}", thickness);
        TGX_ASSERT_MSG(std::isfinite(rotation), "rotation {}", rotation);

        if (radii.x == 0.f || radii.y == 0.f || thickness <= 0.f) {
            return;
        }
        // Thick enough, the outline fills the ellipse.
        if (thickness >= std::min(radii.x, radii.y)) {
            ellipse(center, radii, color, rotation);
            return;
        }

        const BatchState state = state_for(0);
        const std::size_t n = segments(std::max(radii.x, radii.y), state);
        const Vec2 turn = from_angle(rotation);
        const detail::EllipseRim outer{center, radii, turn, n};
        const detail::EllipseRim inner{.center = center, .radii = radii, .turn = turn, .n = n, .inset = thickness};
        push_band(state, n, outer, inner, color, true);
    }

    auto Canvas::poly(Vec2 center, int sides, float radius, float rotation, Color color) noexcept -> void {
        TGX_ASSERT_MSG(detail::valid_radius(radius), "polygon of radius {}", radius);

        const std::size_t n = poly_sides(sides);
        if (radius == 0.f) {
            return;
        }
        const detail::Arc arc{.start = rotation, .sweep = detail::whole_turn};
        push_fan(state_for(0), center, n, detail::Rim{center, radius, arc, n}, color, color, true);
    }

    auto Canvas::poly_lines(
        Vec2 center,
        int sides,
        float radius,
        float rotation,
        Color color,
        float thickness
    ) noexcept -> void {
        TGX_ASSERT_MSG(detail::valid_radius(radius), "polygon of radius {}", radius);
        TGX_ASSERT_MSG(thickness >= 0.f, "thickness {}", thickness);

        const std::size_t n = poly_sides(sides);
        if (radius == 0.f || thickness <= 0.f) {
            return;
        }
        // Thick enough, the outline fills the polygon.
        const float inradius = radius * std::cos(std::numbers::pi_v<float> / static_cast<float>(n));
        if (thickness >= inradius) {
            poly(center, sides, radius, rotation, color);
            return;
        }

        const detail::Arc arc{.start = rotation, .sweep = detail::whole_turn};
        const detail::Rim corner{center, radius, arc, n};
        push_outline(state_for(0), n, corner, center, inradius, thickness, color);
    }

    auto Canvas::sprite(const Texture &texture, const Sprite &sprite) noexcept -> void {
        TGX_ASSERT_MSG(
            texture.id() != target_.texture,
            "a sprite of the render target the canvas draws into"
        );

        const Size texture_size = texture.size();
        // Not Rect::empty(): a negative size is mirrored, not empty.
        const Rect src = sprite.src.width == 0.f && sprite.src.height == 0.f
            ? Rect{0.f, 0.f, static_cast<float>(texture_size.width), static_cast<float>(texture_size.height)}
            : sprite.src;

        const bool natural = sprite.size == Vec2{};
        const float width = natural ? std::abs(src.width) : sprite.size.x;
        const float height = natural ? std::abs(src.height) : sprite.size.y;

        // A negative size reverses the texels, mirroring them.
        float u0 = src.x / static_cast<float>(texture_size.width);
        float v0 = src.y / static_cast<float>(texture_size.height);
        float u1 = (src.x + std::abs(src.width)) / static_cast<float>(texture_size.width);
        float v1 = (src.y + std::abs(src.height)) / static_cast<float>(texture_size.height);
        if (src.width < 0.f) {
            std::swap(u0, u1);
        }
        if (src.height < 0.f) {
            std::swap(v0, v1);
        }
        // A render target's rows go bottom to top.
        if (texture.bottom_up()) {
            v0 = 1.f - v0;
            v1 = 1.f - v1;
        }
        push_quad(
            state_for(texture.id()),
            detail::placed_corners({width, height}, sprite.origin, sprite.position, sprite.rotation),
            detail::corners({u0, v0}, {u1, v1}),
            {sprite.tint, sprite.tint, sprite.tint, sprite.tint}
        );
    }

    auto Canvas::text(Vec2 position, std::string_view text, Color color, float size, Vec2 anchor) noexcept -> void {
        if (anchor != Vec2{}) {
            // Line by line, each shifted by its own width.
            float y = position.y - std::round(measure_text(text, size).y * anchor.y);
            std::size_t begin = 0;
            while (true) {
                const std::size_t end = text.find('\n', begin);
                const std::string_view line = text.substr(begin, end == std::string_view::npos ? end : end - begin);
                const float x = position.x - std::round(measure_text(line, size).x * anchor.x);
                this->text({x, y}, line, color, size);
                if (end == std::string_view::npos) {
                    return;
                }
                y += size;
                begin = end + 1;
            }
        }

        const float scale = size / static_cast<float>(font::line_height);
        const BatchState state = state_for(0);

        Vec2 pen = position;
        for_each_glyph(text, [&](std::size_t glyph_index) noexcept {
            if (glyph_index == newline) {
                pen = {position.x, pen.y + size};
                return;
            }

            const font::Glyph glyph = font::glyphs[glyph_index];
            const float width = static_cast<float>(glyph.width) * scale;
            if (glyph_index != space) {
                const float u0 = static_cast<float>(glyph.x) / font::atlas_width;
                const float u1 = static_cast<float>(glyph.x + glyph.width) / font::atlas_width;
                push_quad(
                    state,
                    detail::corners(pen, pen + Vec2{width, size}),
                    detail::corners({u0, 0.f}, {u1, 1.f}),
                    {color, color, color, color}
                );
            }
            pen.x += width;
        });
    }

    auto Canvas::measure_text(std::string_view text, float size) noexcept -> Vec2 {
        const float scale = size / static_cast<float>(font::line_height);
        float widest = 0.f;
        float line = 0.f;
        float lines = 1.f;
        for_each_glyph(text, [&](std::size_t glyph_index) noexcept {
            if (glyph_index == newline) {
                line = 0.f;
                lines += 1.f;
            } else {
                line += static_cast<float>(font::glyphs[glyph_index].width) * scale;
                widest = std::max(widest, line);
            }
        });
        return {widest, lines * size};
    }

    auto Canvas::fps(Vec2 position, float size) noexcept -> void {
        // Rounded first, so "30 fps" is never orange.
        const float value = std::round(detail::context().clock.fps);
        const Color color = value >= 30.f ? colors::green
            : value >= 15.f ? colors::orange
            : colors::red;

        // On the stack: no allocation every frame.
        std::array<char, 32> buffer{};
        const auto result = std::format_to_n(buffer.data(), buffer.size(), "{:.0f} fps", value);
        const auto length = static_cast<std::size_t>(result.out - buffer.data());
        text(position, std::string_view{buffer.data(), length}, color, size);
    }

    Canvas::Canvas(Size size) noexcept : size_{size} {
    }

    auto Canvas::state_for(GlId texture) noexcept -> detail::BatchState {
        // A canvas that follows the window notices a resize here.
        const Surface surface = this->surface();
        if (const Size span = detail::span_of(size_, viewport_, surface.units);
            transform_stale_ || span != transform_size_) {
            refit(span);
        }
        return {
            .target = target_,
            .texture = texture,
            .blend = blend_,
            .program = program_,
            .u_projection = u_projection_,
            .u_texture = u_texture_,
            .transform = transform_,
            .viewport = pixel_viewport(shown(size_, viewport_, surface), surface),
        };
    }

    auto Canvas::quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) noexcept -> void {
        quad({a, b, c, d}, {color, color, color, color});
    }

    auto Canvas::quad(const std::array<Vec2, 4> &corners, const std::array<Color, 4> &colors) noexcept -> void {
        push_quad(state_for(0), corners, {white_uv, white_uv, white_uv, white_uv}, colors);
    }

    auto Canvas::strip(std::span<const Vec2> points, Color color, float thickness, bool closed) noexcept -> void {
        TGX_ASSERT_MSG(thickness >= 0.f, "thickness {}", thickness);

        const float half = thickness / 2.f;
        const std::size_t n = points.size();
        if (half <= 0.f || n == 0) {
            return;
        }

        // A repeated point has no direction and is skipped. In a loop, the
        // one before the first is the last.
        const auto kept = [&](std::size_t i) noexcept {
            if (i == 0 && !closed) {
                return true;
            }
            return points[i] != points[(i + n - 1) % n];
        };
        // after(i): the next kept point; n at a strip's end, the first kept
        // point in a loop.
        std::size_t first = 0;
        while (first < n && !kept(first)) {
            ++first;
        }
        // A loop of one repeated point.
        if (first == n) {
            return;
        }
        const auto after = [&](std::size_t i) noexcept {
            std::size_t j = i + 1;
            while (j < n && !kept(j)) {
                ++j;
            }
            return closed && j == n ? first : j;
        };
        // A loop of two points is one line, drawn once so a transparent
        // color does not double.
        if (closed && after(after(first)) == first) {
            line(points[first], points[after(first)], color, thickness);
            return;
        }

        struct Line {
            Vec2 direction{};
            float length{};
        };
        const auto line_between = [&](std::size_t from, std::size_t to) noexcept {
            const float length = tgx::length(points[to] - points[from]);
            return Line{(points[to] - points[from]) / length, length};
        };
        // At most half of either line, so the joints at both ends of a line
        // cannot cross.
        const auto joint_at = [&](std::size_t at, const Line &in, const Line &out) noexcept {
            return detail::joint(points[at], in.direction, out.direction, half, std::min(in.length, out.length) / 2.f);
        };

        std::size_t a = first;
        std::size_t b = after(a);
        // A strip of one point.
        if (b == n) {
            return;
        }
        Line current = line_between(a, b);

        // A loop starts at a corner too: the one with the closing line.
        std::optional<detail::Joint> start{};
        if (closed) {
            std::size_t last = n - 1;
            while (!kept(last)) {
                --last;
            }
            start = joint_at(a, line_between(last, a), current);
        }

        while (true) {
            const Vec2 side = perpendicular(current.direction) * half;
            const std::size_t c = after(b);

            // Shared with the next line if mitered; otherwise square, with
            // the outside of the corner filled in.
            std::optional<Line> next{};
            std::optional<detail::Joint> end{};
            if (c < n) {
                next = line_between(b, c);
                end = joint_at(b, current, *next);
                if (!end) {
                    const Vec2 next_side = perpendicular(next->direction) * half;
                    // The left side when the path turns clockwise on
                    // screen.
                    const float outside = cross(current.direction, next->direction) > 0.f ? 1.f : -1.f;
                    triangle(points[b], points[b] + side * outside, points[b] + next_side * outside, color);
                }
            }

            const detail::Joint from = start.value_or(detail::Joint{points[a] + side, points[a] - side});
            const detail::Joint to = end.value_or(detail::Joint{points[b] + side, points[b] - side});
            quad(from.plus, to.plus, to.minus, from.minus, color);

            // A strip ends at its last point, a loop back at its first.
            if (!next || b == first) {
                break;
            }
            start = end;
            current = *next;
            a = b;
            b = c;
        }
    }

    auto Canvas::segments(float radius, const detail::BatchState &state) const noexcept -> std::size_t {
        // One unit is pixels / span along the most stretched axis.
        float pixels_per_unit = 1.f;
        if (!transform_size_.empty()) {
            pixels_per_unit = std::max(
                static_cast<float>(state.viewport.width) / static_cast<float>(transform_size_.width),
                static_cast<float>(state.viewport.height) / static_cast<float>(transform_size_.height)
            );
        }
        return detail::segments_for(radius * std::abs(camera_.zoom) * pixels_per_unit);
    }

    auto Canvas::surface() const noexcept -> detail::Surface {
        return detail::surface_of(target_);
    }

    auto Canvas::refit(Size span) noexcept -> void {
        transform_size_ = span;
        transform_ = projection_for(span) * camera_.matrix();
        transform_stale_ = false;
    }
}
