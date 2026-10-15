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
    using tgx::detail::BatchVertex;
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
        tgx::Vec2 offset;
        tgx::Vec2 scale;

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
        const tgx::Rect area = covered(viewport, surface);
        const tgx::Size span = tgx::detail::span_of(size, viewport, surface.units);
        if (area.empty() || span.empty()) {
            return std::nullopt;
        }
        return ScreenMapping{
            {area.x, area.y},
            {static_cast<float>(span.width) / area.width, static_cast<float>(span.height) / area.height},
        };
    }

    // Radians.
    constexpr float whole_turn = 2.f * std::numbers::pi_v<float>;

    // Start angle and clockwise sweep, at most a full turn.
    struct Arc {
        float start;
        float sweep;
    };

    [[nodiscard]] auto arc_between(float start, float end) noexcept -> Arc {
        if (end < start) {
            std::swap(start, end);
        }
        return {start, std::min(end - start, whole_turn)};
    }

    // Segments for the arc, given n for the whole circle. At least one.
    [[nodiscard]] auto arc_segments(const Arc &arc, std::size_t n) noexcept -> std::size_t {
        const float part = arc.sweep / whole_turn;
        return std::max(static_cast<std::size_t>(std::ceil(part * static_cast<float>(n))), std::size_t{1});
    }

    // A full circle's last point is its first; a closed fan or band reuses
    // it exactly, leaving no seam.
    [[nodiscard]] auto full_turn(const Arc &arc) noexcept -> bool {
        return arc.sweep >= whole_turn;
    }

    // As circle() draws it.
    constexpr Arc full_circle{0.f, whole_turn};

    // Point i of a unit-circle arc split into n segments.
    [[nodiscard]] auto arc_point(const Arc &arc, std::size_t i, std::size_t n) noexcept -> tgx::Vec2 {
        return tgx::from_angle(arc.start + arc.sweep * static_cast<float>(i) / static_cast<float>(n));
    }

    // The points of an arc split into n segments, radius around center, as
    // push_fan and push_band take them.
    struct Rim {
        tgx::Vec2 center;
        float radius;
        Arc arc;
        std::size_t n;

        [[nodiscard]] auto operator()(std::size_t i) const noexcept -> tgx::Vec2 {
            return center + arc_point(arc, i, n) * radius;
        }
    };

    // One axis of an EllipseRim point moved inset along the normal, kept on
    // its own side and within radius - inset: where the edge bends tighter
    // than inset, the moved points would cross over.
    [[nodiscard]] auto inward(float at, float normal, float radius, float inset) noexcept -> float {
        const float limit = radius - inset;
        const float moved = at - normal * inset;
        return at < 0.f ? std::clamp(moved, -limit, 0.f) : std::clamp(moved, 0.f, limit);
    }

    // As Rim, for a whole ellipse turned by an angle of cosine and sine turn.
    // inset moves the points inward, for an outline; under min(radii).
    struct EllipseRim {
        tgx::Vec2 center;
        tgx::Vec2 radii;
        tgx::Vec2 turn;
        std::size_t n;
        float inset = 0.f;

        [[nodiscard]] auto operator()(std::size_t i) const noexcept -> tgx::Vec2 {
            const tgx::Vec2 unit = arc_point(full_circle, i, n);
            tgx::Vec2 at{unit.x * radii.x, unit.y * radii.y};
            if (inset > 0.f) {
                const tgx::Vec2 normal = tgx::normalize(tgx::Vec2{unit.x / radii.x, unit.y / radii.y});
                at = {inward(at.x, normal.x, radii.x, inset), inward(at.y, normal.y, radii.y, inset)};
            }
            return center + tgx::Vec2{at.x * turn.x - at.y * turn.y, at.x * turn.y + at.y * turn.x};
        }
    };

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

    // Clockwise from the top-left, as push_quad takes them.
    [[nodiscard]] constexpr auto corners(
        tgx::Vec2 top_left,
        tgx::Vec2 bottom_right
    ) noexcept -> std::array<tgx::Vec2, 4> {
        return {top_left, {bottom_right.x, top_left.y}, bottom_right, {top_left.x, bottom_right.y}};
    }

    [[nodiscard]] constexpr auto corners(tgx::Rect rect) noexcept -> std::array<tgx::Vec2, 4> {
        return corners({rect.x, rect.y}, {rect.right(), rect.bottom()});
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

    // Clockwise from the top-left. origin, from the top-left corner, is
    // placed at position, and the box rotates around it.
    [[nodiscard]] auto placed_corners(
        tgx::Vec2 size,
        tgx::Vec2 origin,
        tgx::Vec2 position,
        float rotation
    ) noexcept -> std::array<tgx::Vec2, 4> {
        auto box = corners(-origin, size - origin);
        if (rotation == 0.f) {
            for (tgx::Vec2 &corner: box) {
                corner += position;
            }
            return box;
        }
        // rotate() written out, to compute cos and sin once for all corners.
        const float c = std::cos(rotation);
        const float s = std::sin(rotation);
        for (tgx::Vec2 &corner: box) {
            corner = tgx::Vec2{corner.x * c - corner.y * s, corner.x * s + corner.y * c} + position;
        }
        return box;
    }

    // Point i on the edge of a rounded rectangle, clockwise from the left end
    // of the top-left corner. inset moves the edge inward, for an outline:
    // corners keep their centers, and are square once inset reaches radius.
    [[nodiscard]] auto rounded_rim(
        tgx::Rect rect,
        float radius,
        float inset,
        std::size_t points,
        std::size_t i
    ) noexcept -> tgx::Vec2 {
        const std::size_t corner = i / points;
        const std::size_t step = i % points;
        const float in = std::max(radius, inset);
        const auto centers = corners({rect.x + in, rect.y + in}, {rect.right() - in, rect.bottom() - in});
        // The top-left quarter starts at half a turn (left); each next corner
        // a quarter turn later.
        constexpr float quarter = whole_turn / 4.f;
        const Arc arc{quarter * static_cast<float>(corner + 2), quarter};
        return centers[corner] + arc_point(arc, step, points - 1) * std::max(radius - inset, 0.f);
    }

    // Per rounded corner, given n segments for the whole circle.
    [[nodiscard]] auto corner_points(std::size_t n) noexcept -> std::size_t {
        return arc_segments({0.f, whole_turn / 4.f}, n) + 1;
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

        m_size = size;
        m_transform_stale = true;
    }

    auto Canvas::size() const noexcept -> Size {
        return detail::span_of(m_size, m_viewport, surface().units);
    }

    auto Canvas::set_viewport(Rect rect) noexcept -> void {
        // A negative size would silently count as empty.
        TGX_ASSERT_MSG(
            std::isfinite(rect.x) && std::isfinite(rect.y) && rect.width >= 0.f && rect.height >= 0.f
                && std::isfinite(rect.width) && std::isfinite(rect.height),
            "viewport {} {} {}x{}",
            rect.x, rect.y, rect.width, rect.height
        );

        m_viewport = rect;
        m_transform_stale = true;
    }

    auto Canvas::set_target(const RenderTarget *target) noexcept -> void {
        m_target = detail::target_of(target);
        m_transform_stale = true;
    }

    auto Canvas::set_camera(const Camera2D &camera) noexcept -> void {
        TGX_ASSERT_MSG(camera.zoom != 0.f, "a camera with zoom 0 shows nothing and cannot map back");
        TGX_ASSERT_MSG(
            std::isfinite(camera.zoom) && std::isfinite(camera.rotation),
            "a camera with zoom {} and rotation {}",
            camera.zoom, camera.rotation
        );

        m_camera = camera;
        m_transform_stale = true;
    }

    auto Canvas::to_world(Vec2 point) const noexcept -> Vec2 {
        const auto mapping = screen_mapping(m_size, m_viewport, surface());
        return m_camera.to_world(mapping ? mapping->to_canvas(point) : point);
    }

    auto Canvas::to_screen(Vec2 world) const noexcept -> Vec2 {
        const auto mapping = screen_mapping(m_size, m_viewport, surface());
        const Vec2 point = m_camera.to_screen(world);
        return mapping ? mapping->to_screen(point) : point;
    }

    auto Canvas::set_shader(const gl::Shader *shader) noexcept -> void {
        m_program = shader ? shader->id() : 0;
        m_u_projection = -1;
        m_u_texture = -1;
        if (!shader) {
            return;
        }

        // The only required uniform; the lookup asserts it is a mat4.
        m_u_projection = gl::detail::location(shader->uniform<Mat4>("u_projection"));

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
            m_u_texture = gl::detail::location(shader->uniform<gl::TextureSlot>("u_texture"));
        }
    }

    auto Canvas::clear(Color color) noexcept -> void {
        detail::context().clear({.target = m_target, .color = color});
    }

    auto Canvas::rect(Rect rect, Color color) noexcept -> void {
        quad(corners(rect), {color, color, color, color});
    }

    auto Canvas::rect(Rect rect, Vec2 origin, float rotation, Color color) noexcept -> void {
        const auto box = placed_corners({rect.width, rect.height}, origin, {rect.x, rect.y}, rotation);
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
        const std::size_t points = corner_points(segments(r, state));
        const auto rim = [&](std::size_t i) noexcept { return rounded_rim(rect, r, 0.f, points, i); };
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
        const std::size_t points = corner_points(segments(r, state));
        push_band(
            state,
            points * 4,
            [&](std::size_t i) noexcept { return rounded_rim(rect, r, 0.f, points, i); },
            [&](std::size_t i) noexcept { return rounded_rim(rect, r, thickness, points, i); },
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
        const auto box = corners(rect);
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
        push_fan(state, center, n, Rim{center, radius, full_circle, n}, inner, outer, true);
    }

    auto Canvas::circle_lines(Vec2 center, float radius, Color color, float thickness) noexcept -> void {
        TGX_ASSERT_MSG(detail::valid_radius(radius), "circle of radius {}", radius);
        TGX_ASSERT_MSG(thickness >= 0.f, "thickness {}", thickness);

        // Thick enough, the ring has no hole.
        ring(center, std::max(radius - thickness, 0.f), radius, full_circle.start, full_circle.sweep, color);
    }

    auto Canvas::circle_sector(Vec2 center, float radius, float start, float end, Color color) noexcept -> void {
        TGX_ASSERT_MSG(detail::valid_radius(radius), "circle of radius {}", radius);
        TGX_ASSERT_MSG(std::isfinite(start) && std::isfinite(end), "sector from {} to {}", start, end);

        const Arc arc = arc_between(start, end);
        if (radius == 0.f || arc.sweep == 0.f) {
            return;
        }

        const BatchState state = state_for(0);
        const std::size_t n = arc_segments(arc, segments(radius, state));
        const bool full = full_turn(arc);
        push_fan(state, center, n, Rim{center, radius, arc, n}, color, color, full);
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

        const Arc arc = arc_between(start, end);
        if (inner_radius == outer_radius || arc.sweep == 0.f) {
            return;
        }

        const BatchState state = state_for(0);
        const std::size_t n = arc_segments(arc, segments(outer_radius, state));
        const bool full = full_turn(arc);
        push_band(
            state,
            n,
            Rim{center, outer_radius, arc, n},
            Rim{center, inner_radius, arc, n},
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
        const EllipseRim rim{center, radii, from_angle(rotation), n};
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
        push_band(state, n, EllipseRim{center, radii, turn, n}, EllipseRim{center, radii, turn, n, thickness}, color, true);
    }

    auto Canvas::poly(Vec2 center, int sides, float radius, float rotation, Color color) noexcept -> void {
        TGX_ASSERT_MSG(detail::valid_radius(radius), "polygon of radius {}", radius);

        const std::size_t n = poly_sides(sides);
        if (radius == 0.f) {
            return;
        }
        const Arc arc{rotation, whole_turn};
        push_fan(state_for(0), center, n, Rim{center, radius, arc, n}, color, color, true);
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

        const Arc arc{rotation, whole_turn};
        const Rim corner{center, radius, arc, n};
        push_outline(state_for(0), n, corner, center, inradius, thickness, color);
    }

    auto Canvas::sprite(const Texture &texture, const Sprite &sprite) noexcept -> void {
        TGX_ASSERT_MSG(
            texture.id() != m_target.texture,
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
            placed_corners({width, height}, sprite.origin, sprite.position, sprite.rotation),
            corners({u0, v0}, {u1, v1}),
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
                    corners(pen, pen + Vec2{width, size}),
                    corners({u0, 0.f}, {u1, 1.f}),
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

    Canvas::Canvas(Size size) noexcept : m_size{size} {
    }

    auto Canvas::state_for(GlId texture) noexcept -> detail::BatchState {
        // A canvas that follows the window notices a resize here.
        const Surface surface = this->surface();
        if (const Size span = detail::span_of(m_size, m_viewport, surface.units);
            m_transform_stale || span != m_transform_size) {
            refit(span);
        }
        return {
            .target = m_target,
            .texture = texture,
            .blend = m_blend,
            .program = m_program,
            .u_projection = m_u_projection,
            .u_texture = m_u_texture,
            .transform = m_transform,
            .viewport = pixel_viewport(m_viewport, surface),
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
            Vec2 direction;
            float length;
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
        std::optional<detail::Joint> start;
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
            std::optional<Line> next;
            std::optional<detail::Joint> end;
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
        if (!m_transform_size.empty()) {
            pixels_per_unit = std::max(
                static_cast<float>(state.viewport.width) / static_cast<float>(m_transform_size.width),
                static_cast<float>(state.viewport.height) / static_cast<float>(m_transform_size.height)
            );
        }
        return detail::segments_for(radius * std::abs(m_camera.zoom) * pixels_per_unit);
    }

    auto Canvas::surface() const noexcept -> detail::Surface {
        return detail::surface_of(m_target);
    }

    auto Canvas::refit(Size span) noexcept -> void {
        m_transform_size = span;
        m_transform = projection_for(span) * m_camera.matrix();
        m_transform_stale = false;
    }
}
