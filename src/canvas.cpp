#include "tgx/canvas.h"

#include "tgx/assert.h"
#include "tgx/handle.h"
#include "tgx/texture.h"

#include "tgx/gl/draw.h"
#include "tgx/gl/shader.h"

#include "batch.h"
#include "context.h"
#include "default_font.h"
#include "geometry.h"

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

    // Where shapes sample the built-in texture: the middle texel of its white
    // block, whose neighbors are white too.
    constexpr tgx::Vec2 white_uv{
        (static_cast<float>(font::white_x) + font::white_size / 2.f) / font::atlas_width,
        (font::white_size / 2.f) / font::atlas_height,
    };

    // Glyphs fill the atlas top to bottom, one row of them: v runs 0 to 1.
    static_assert(font::atlas_height == font::line_height);

    // Stands for a line break among glyph indices.
    constexpr std::size_t newline = font::glyphs.size();

    // The glyph of ' ': all advance and no ink.
    constexpr std::size_t space = static_cast<std::size_t>(' ' - font::first);

    // How many bytes the UTF-8 sequence its lead byte starts is long: as many
    // as its leading ones, none for ASCII. A stray continuation byte (one
    // leading one) or an invalid lead is one of its own.
    [[nodiscard]] constexpr auto sequence_length(unsigned char lead) noexcept -> std::size_t {
        const int ones = std::countl_one(lead);
        return ones >= 2 && ones <= 4 ? static_cast<std::size_t>(ones) : 1;
    }

    // Calls f with the index of each character's glyph in the default font,
    // or newline. UTF-8 sequences are one character each: those the font has
    // no glyph for show as '?'.
    template<typename F>
    auto for_each_glyph(std::string_view text, F f) noexcept -> void {
        constexpr auto index = [](char c) noexcept {
            return static_cast<std::size_t>(c - font::first);
        };
        for (std::size_t i = 0; i < text.size();) {
            const auto c = static_cast<unsigned char>(text[i]);
            // Only continuation bytes are taken after the lead one, so a
            // sequence cut short does not swallow the next character.
            const std::size_t length = sequence_length(c);
            ++i;
            for (std::size_t taken = 1; taken < length && i < text.size(); ++taken, ++i) {
                if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
                    break;
                }
            }

            if (c == '\n') {
                f(newline);
            } else if (c >= static_cast<unsigned char>(font::first) && c <= static_cast<unsigned char>(font::last)) {
                f(index(static_cast<char>(c)));
            } else {
                f(index('?'));
            }
        }
    }

    // The canvas spans the size with y down: (0, 0) at the top-left. An empty
    // size (a minimized window) would divide by zero; nothing shows then anyway.
    [[nodiscard]] constexpr auto projection_for(tgx::Size size) noexcept -> tgx::Mat4 {
        if (size.empty()) {
            return {};
        }
        return tgx::ortho(0.f, static_cast<float>(size.width), static_cast<float>(size.height), 0.f);
    }

    // The part of the surface a canvas covers, in its units: the viewport, or
    // all of it.
    [[nodiscard]] auto covered(tgx::Rect viewport, const Surface &surface) noexcept -> tgx::Rect {
        if (!viewport.empty()) {
            return viewport;
        }
        return {0.f, 0.f, static_cast<float>(surface.units.width), static_cast<float>(surface.units.height)};
    }

    // The part of the surface a canvas covers, in pixels: its viewport, from
    // units, or all of it.
    [[nodiscard]] auto pixel_viewport(tgx::Rect rect, const Surface &surface) noexcept -> tgx::gl::Viewport {
        if (rect.empty() || surface.units.empty()) {
            return surface.viewport();
        }

        // Edges rounded rather than sizes, so canvases side by side meet
        // without a gap or an overlap.
        const float sx = static_cast<float>(surface.pixels.width) / static_cast<float>(surface.units.width);
        const float sy = static_cast<float>(surface.pixels.height) / static_cast<float>(surface.units.height);
        const auto left = static_cast<int>(std::lround(rect.x * sx));
        const auto top = static_cast<int>(std::lround(rect.y * sy));
        const auto right = static_cast<int>(std::lround(rect.right() * sx));
        const auto bottom = static_cast<int>(std::lround(rect.bottom() * sy));
        return {left, top, right - left, bottom - top};
    }

    // Segments for a circle of the radius, by how many pixels it shows over:
    // one unit of the canvas covers pixels / span along the axis it is
    // stretched most, so a canvas of its own size stretched over more
    // pixels, or a scaling display, shows its circles bigger than their
    // radius. span is the one the state was made for.
    [[nodiscard]] auto circle_segments(
        float radius,
        float zoom,
        const BatchState &state,
        tgx::Size span
    ) noexcept -> std::size_t {
        float pixels_per_unit = 1.f;
        if (!span.empty()) {
            pixels_per_unit = std::max(
                static_cast<float>(state.viewport.width) / static_cast<float>(span.width),
                static_cast<float>(state.viewport.height) / static_cast<float>(span.height)
            );
        }
        return tgx::detail::segments_for(radius * std::abs(zoom) * pixels_per_unit);
    }

    // From the part of the surface a canvas covers to the canvas's own
    // coordinates: canvas = (screen - offset) * scale, axis by axis. None for
    // a minimized window, which covers nothing to map from.
    struct ScreenMapping {
        tgx::Vec2 offset;
        tgx::Vec2 scale;
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

    // The unit circle split into n, from angle 0 clockwise on screen.
    [[nodiscard]] auto rim_point(std::size_t i, std::size_t n) noexcept -> tgx::Vec2 {
        return tgx::from_angle(2.f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(n));
    }

    // The Device's batch, with room for this many more vertices and indices
    // drawn with the state, and the index the first of them gets.
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

    // Four corners clockwise, as two triangles: a b c and c d a. Shapes,
    // sprites and glyphs all come down to it.
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
}

namespace tgx {
    auto Canvas::create(Size size) noexcept -> Canvas {
        return Canvas{size};
    }

    auto Canvas::set_size(Size size) noexcept -> void {
        m_size = size;
        m_transform_stale = true;
    }

    auto Canvas::size() const noexcept -> Size {
        return detail::span_of(m_size, m_viewport, surface().units);
    }

    auto Canvas::set_viewport(Rect rect) noexcept -> void {
        m_viewport = rect;
        m_transform_stale = true;
    }

    auto Canvas::set_target(const RenderTarget *target) noexcept -> void {
        m_target = detail::target_of(target);
        m_transform_stale = true;
    }

    auto Canvas::set_camera(const Camera2D &camera) noexcept -> void {
        TGX_ASSERT_MSG(camera.zoom != 0.f, "a camera with zoom 0 shows nothing and cannot map back");

        m_camera = camera;
        m_transform_stale = true;
    }

    auto Canvas::to_world(Vec2 point) const noexcept -> Vec2 {
        const auto mapping = screen_mapping(m_size, m_viewport, surface());
        if (!mapping) {
            return m_camera.to_world(point);
        }
        const Vec2 from_corner = point - mapping->offset;
        return m_camera.to_world({from_corner.x * mapping->scale.x, from_corner.y * mapping->scale.y});
    }

    auto Canvas::to_screen(Vec2 world) const noexcept -> Vec2 {
        const auto mapping = screen_mapping(m_size, m_viewport, surface());
        const Vec2 canvas_point = m_camera.to_screen(world);
        if (!mapping) {
            return canvas_point;
        }
        return Vec2{canvas_point.x / mapping->scale.x, canvas_point.y / mapping->scale.y} + mapping->offset;
    }

    auto Canvas::set_shader(const gl::Shader *shader) noexcept -> void {
        m_program = shader ? shader->id() : 0;
        m_u_projection = -1;
        m_u_texture = -1;
        if (!shader) {
            return;
        }

        // The projection is the one uniform the canvas needs; the lookup
        // asserts it exists and is a mat4.
        m_u_projection = gl::detail::location(shader->uniform<Mat4>("u_projection"));

        // The texture is optional: a shader may ignore it. The batch draws
        // without Device::draw's checks, so the one on its samplers is here.
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
        quad(
            {rect.x, rect.y}, {rect.right(), rect.y},
            {rect.right(), rect.bottom()}, {rect.x, rect.bottom()},
            color
        );
    }

    auto Canvas::rect_lines(Rect rect, Color color, float thickness) noexcept -> void {
        // Thicker than half the rectangle, the frame is the rectangle.
        const float t = std::min({thickness, rect.width / 2.f, rect.height / 2.f});
        if (t <= 0.f) {
            return;
        }

        // Top and bottom full width, the sides between them, so no corner is
        // covered twice: a see-through frame stays even.
        this->rect({rect.x, rect.y, rect.width, t}, color);
        this->rect({rect.x, rect.bottom() - t, rect.width, t}, color);
        this->rect({rect.x, rect.y + t, t, rect.height - 2.f * t}, color);
        this->rect({rect.right() - t, rect.y + t, t, rect.height - 2.f * t}, color);
    }

    auto Canvas::rect_gradient(Rect rect, Color top, Color bottom) noexcept -> void {
        rect_gradient(rect, top, top, bottom, bottom);
    }

    auto Canvas::rect_gradient(
        Rect rect,
        Color top_left,
        Color top_right,
        Color bottom_right,
        Color bottom_left
    ) noexcept -> void {
        const std::array<Vec2, 4> corners{
            Vec2{rect.x, rect.y},
            Vec2{rect.right(), rect.y},
            Vec2{rect.right(), rect.bottom()},
            Vec2{rect.x, rect.bottom()},
        };
        const std::array<Color, 4> colors{top_left, top_right, bottom_right, bottom_left};
        if (detail::blends_flat(colors)) {
            quad(corners, colors);
            return;
        }

        // Four triangles around the middle, so no one diagonal shows: red
        // and green crossed would otherwise leave a band along it.
        auto [batch, first] = start(state_for(0), 5, 12);
        batch.push_vertex({rect.center(), white_uv, detail::average(colors)});
        for (std::size_t i = 0; i < 4; ++i) {
            batch.push_vertex({corners[i], white_uv, colors[i]});
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

    auto Canvas::line(Vec2 a, Vec2 b, Color color, float thickness) noexcept -> void {
        // A point has no direction to widen it across.
        const Vec2 along = b - a;
        if (along == Vec2{} || thickness <= 0.f) {
            return;
        }

        const Vec2 side = perpendicular(normalize(along)) * (thickness / 2.f);
        quad(a + side, b + side, b - side, a - side, color);
    }

    auto Canvas::line_strip(std::span<const Vec2> points, Color color, float thickness) noexcept -> void {
        const float half = thickness / 2.f;
        if (half <= 0.f) {
            return;
        }

        // A point that repeats the one before has no direction: skipped.
        const auto next_distinct = [&](std::size_t i) noexcept {
            std::size_t j = i + 1;
            while (j < points.size() && points[j] == points[i]) {
                ++j;
            }
            return j;
        };

        std::size_t a = 0;
        std::size_t b = next_distinct(a);
        Vec2 previous{};
        float previous_length = 0.f;
        bool has_previous = false;
        while (b < points.size()) {
            const std::size_t c = next_distinct(b);
            const float length = tgx::length(points[b] - points[a]);
            const Vec2 direction = (points[b] - points[a]) / length;
            const Vec2 side = perpendicular(direction) * half;

            // Its start: where the joint before left it, or square.
            detail::Joint start{points[a] + side, points[a] - side, true};
            if (has_previous) {
                // Half of either line each, so the joints at its two ends
                // cannot cross.
                start = detail::joint(points[a], previous, direction, half, std::min(previous_length, length) / 2.f);
                if (!start.mitered) {
                    start = {points[a] + side, points[a] - side, true};
                }
            }

            // Its end: shared with the next line when the corner is mitered;
            // square otherwise, with the corner's outside filled in.
            detail::Joint end{points[b] + side, points[b] - side, true};
            if (c < points.size()) {
                const float next_length = tgx::length(points[c] - points[b]);
                const Vec2 next = (points[c] - points[b]) / next_length;
                const detail::Joint corner = detail::joint(
                    points[b], direction, next, half, std::min(length, next_length) / 2.f
                );
                if (corner.mitered) {
                    end = corner;
                } else {
                    const Vec2 next_side = perpendicular(next) * half;
                    // The outside is where the lines' edges part: the left
                    // when the path turns clockwise on screen.
                    const float outside = cross(direction, next) > 0.f ? 1.f : -1.f;
                    triangle(points[b], points[b] + side * outside, points[b] + next_side * outside, color);
                }
            }

            quad(start.plus, end.plus, end.minus, start.minus, color);

            previous = direction;
            previous_length = length;
            has_previous = true;
            a = b;
            b = c;
        }
    }

    auto Canvas::circle(Vec2 center, float radius, Color color) noexcept -> void {
        circle_gradient(center, radius, color, color);
    }

    auto Canvas::circle_gradient(Vec2 center, float radius, Color inner, Color outer) noexcept -> void {
        if (radius <= 0.f) {
            return;
        }

        // state_for brings the span up to date first.
        const BatchState state = state_for(0);
        const std::size_t n = circle_segments(radius, m_camera.zoom, state, m_transform_size);
        auto [batch, first] = start(state, n + 1, n * 3);

        // A fan around the center, vertex first.
        batch.push_vertex({center, white_uv, inner});
        for (std::size_t i = 0; i < n; ++i) {
            batch.push_vertex({center + rim_point(i, n) * radius, white_uv, outer});
        }
        for (std::size_t i = 0; i < n; ++i) {
            push_indices(batch, first, {0, 1 + i, 1 + (i + 1) % n});
        }
    }

    auto Canvas::circle_lines(Vec2 center, float radius, Color color, float thickness) noexcept -> void {
        if (radius <= 0.f || thickness <= 0.f) {
            return;
        }
        // Thicker than the radius, the ring is the circle.
        if (thickness >= radius) {
            circle(center, radius, color);
            return;
        }

        const float inner = radius - thickness;
        // state_for brings the span up to date first.
        const BatchState state = state_for(0);
        const std::size_t n = circle_segments(radius, m_camera.zoom, state, m_transform_size);
        auto [batch, first] = start(state, n * 2, n * 6);

        // Outer and inner rim points in pairs; each pair and the next make a
        // quad of the ring.
        for (std::size_t i = 0; i < n; ++i) {
            const Vec2 direction = rim_point(i, n);
            batch.push_vertex({center + direction * radius, white_uv, color});
            batch.push_vertex({center + direction * inner, white_uv, color});
        }
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t outer = 2 * i;
            const std::size_t next = 2 * ((i + 1) % n);
            push_indices(batch, first, {outer, next, next + 1, next + 1, outer + 1, outer});
        }
    }

    auto Canvas::sprite(const Texture &texture, const Sprite &sprite) noexcept -> void {
        TGX_ASSERT_MSG(
            texture.id() != m_target.texture,
            "a sprite of the render target the canvas draws into"
        );

        const Size texture_size = texture.size();
        // An src left unset is the whole texture. Not Rect::empty(): a
        // negative size is a mirrored part, not none.
        const Rect src = sprite.src.width == 0.f && sprite.src.height == 0.f
            ? Rect{0.f, 0.f, static_cast<float>(texture_size.width), static_cast<float>(texture_size.height)}
            : sprite.src;

        const bool natural = sprite.size == Vec2{};
        const float width = natural ? std::abs(src.width) : sprite.size.x;
        const float height = natural ? std::abs(src.height) : sprite.size.y;

        // Texture coordinates of src's corners. A negative size takes the same
        // texels, from x to x + |width|, and runs them backwards: a mirror.
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
        // A render target's rows run bottom to top: the same texels, counted
        // from the other end.
        if (texture.bottom_up()) {
            v0 = 1.f - v0;
            v1 = 1.f - v1;
        }
        const std::array<Vec2, 4> uvs{Vec2{u0, v0}, Vec2{u1, v0}, Vec2{u1, v1}, Vec2{u0, v1}};

        // Corners relative to the origin, turned, then put at the position.
        std::array<Vec2, 4> corners{
            Vec2{-sprite.origin.x, -sprite.origin.y},
            Vec2{width - sprite.origin.x, -sprite.origin.y},
            Vec2{width - sprite.origin.x, height - sprite.origin.y},
            Vec2{-sprite.origin.x, height - sprite.origin.y},
        };
        if (sprite.rotation == 0.f) {
            for (Vec2 &corner: corners) {
                corner += sprite.position;
            }
        } else {
            const float c = std::cos(sprite.rotation);
            const float s = std::sin(sprite.rotation);
            for (Vec2 &corner: corners) {
                corner = Vec2{corner.x * c - corner.y * s, corner.x * s + corner.y * c} + sprite.position;
            }
        }

        push_quad(state_for(texture.id()), corners, uvs, {sprite.tint, sprite.tint, sprite.tint, sprite.tint});
    }

    auto Canvas::text(Vec2 position, std::string_view text, Color color, float size) noexcept -> void {
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
                    {pen, pen + Vec2{width, 0.f}, pen + Vec2{width, size}, pen + Vec2{0.f, size}},
                    {Vec2{u0, 0.f}, Vec2{u1, 0.f}, Vec2{u1, 1.f}, Vec2{u0, 1.f}},
                    {color, color, color, color}
                );
            }
            pen.x += width;
        });
    }

    auto Canvas::measure_text(std::string_view text, float size) noexcept -> Vec2 {
        if (text.empty()) {
            return {};
        }

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
        const float value = detail::context().clock.fps;
        const Color color = value >= 30.f ? colors::green
            : value >= 15.f ? colors::orange
            : colors::red;

        // Formatted on the stack: nothing allocated every frame.
        std::array<char, 32> buffer{};
        const auto result = std::format_to_n(buffer.data(), buffer.size(), "{:.0f} fps", value);
        const auto length = static_cast<std::size_t>(result.out - buffer.data());
        text(position, std::string_view{buffer.data(), length}, color, size);
    }

    Canvas::Canvas(Size size) noexcept : m_size{size} {
    }

    auto Canvas::state_for(GlId texture) noexcept -> detail::BatchState {
        // A canvas that follows the window notices a resize here, at its first
        // shape after it.
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

    auto Canvas::surface() const noexcept -> detail::Surface {
        return detail::surface_of(m_target);
    }

    auto Canvas::refit(Size span) noexcept -> void {
        m_transform_size = span;
        m_transform = projection_for(span) * m_camera.matrix();
        m_transform_stale = false;
    }
}
