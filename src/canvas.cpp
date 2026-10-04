#include "tgx/canvas.h"

#include "tgx/assert.h"
#include "tgx/handle.h"
#include "tgx/texture.h"

#include "tgx/gl/draw.h"
#include "tgx/gl/shader.h"

#include "batch.h"
#include "context.h"
#include "default_font.h"
#include "window_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <numbers>
#include <string_view>
#include <utility>

namespace {
    using tgx::detail::Batch;
    using tgx::detail::BatchState;
    using tgx::detail::BatchVertex;

    // Segments for a circle: enough that no edge strays more than a quarter
    // unit from the true circle, as large as it shows on screen.
    constexpr float circle_tolerance = 0.25f;
    constexpr std::size_t min_segments = 8;
    constexpr std::size_t max_segments = 1024;

    static_assert(
        max_segments * 2 <= tgx::detail::batch_max_vertices
        && max_segments * 6 <= tgx::detail::batch_max_indices
    );

    namespace font = tgx::detail::default_font;

    // Where shapes sample the built-in texture: the middle texel of its white
    // block, whose neighbours are white too.
    constexpr tgx::Vec2 white_uv{
        (static_cast<float>(font::white_x) + font::white_size / 2.f) / font::atlas_width,
        (font::white_size / 2.f) / font::atlas_height,
    };

    // Stands for a line break among glyph indices.
    constexpr std::size_t newline = font::glyphs.size();

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
            // The lead byte says how long the sequence is; a stray
            // continuation byte counts as a character of its own. Only
            // continuation bytes are taken after it, so a sequence cut short
            // does not swallow the next character.
            const std::size_t length = c < 0x80 ? 1
                : (c >> 5) == 0x6 ? 2
                : (c >> 4) == 0xE ? 3
                : (c >> 3) == 0x1E ? 4
                : 1;
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

    [[nodiscard]] auto has_area(tgx::Rect rect) noexcept -> bool {
        return rect.width > 0.f && rect.height > 0.f;
    }

    // The part of the window a canvas covers, in its screen coordinates: the
    // viewport, or all of it.
    [[nodiscard]] auto covered(tgx::Rect viewport) noexcept -> tgx::Rect {
        if (has_area(viewport)) {
            return viewport;
        }
        const tgx::Size window = tgx::detail::window_size();
        return {0.f, 0.f, static_cast<float>(window.width), static_cast<float>(window.height)};
    }

    // The part of the framebuffer a canvas covers, in pixels: its viewport,
    // from screen coordinates, or all of it.
    [[nodiscard]] auto pixel_viewport(tgx::Rect rect) noexcept -> tgx::gl::Viewport {
        const tgx::Size window = tgx::detail::window_size();
        const tgx::Size framebuffer = tgx::detail::framebuffer_size();
        if (!has_area(rect) || window.empty()) {
            return {0, 0, framebuffer.width, framebuffer.height};
        }

        // Edges rounded rather than sizes, so canvases side by side meet
        // without a gap or an overlap.
        const float sx = static_cast<float>(framebuffer.width) / static_cast<float>(window.width);
        const float sy = static_cast<float>(framebuffer.height) / static_cast<float>(window.height);
        const auto left = static_cast<int>(std::lround(rect.x * sx));
        const auto top = static_cast<int>(std::lround(rect.y * sy));
        const auto right = static_cast<int>(std::lround(rect.right() * sx));
        const auto bottom = static_cast<int>(std::lround(rect.bottom() * sy));
        return {left, top, right - left, bottom - top};
    }

    [[nodiscard]] auto segments_for(float screen_radius) noexcept -> std::size_t {
        if (screen_radius <= circle_tolerance) {
            return min_segments;
        }
        // An edge of a circle split into n strays r * (1 - cos(pi / n)) from it.
        // A huge radius rounds the cosine to 1 and n to infinity, which no
        // cast survives: clamped while still a float.
        const float n = std::numbers::pi_v<float> / std::acos(1.f - circle_tolerance / screen_radius);
        if (!(n < static_cast<float>(max_segments))) {
            return max_segments;
        }
        return std::max(static_cast<std::size_t>(std::ceil(n)), min_segments);
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
}

namespace tgx {
    auto Canvas::create(Size size) noexcept -> Canvas {
        return Canvas{size};
    }

    Canvas::Canvas(Size size) noexcept : m_size{size} {
        refit();
    }

    auto Canvas::set_size(Size size) noexcept -> void {
        m_size = size;
        refit();
    }

    auto Canvas::size() const noexcept -> Size {
        if (!m_size.empty()) {
            return m_size;
        }
        if (has_area(m_viewport)) {
            return {static_cast<int>(std::lround(m_viewport.width)), static_cast<int>(std::lround(m_viewport.height))};
        }
        return detail::window_size();
    }

    auto Canvas::set_viewport(Rect rect) noexcept -> void {
        m_viewport = rect;
        refit();
    }

    auto Canvas::set_camera(const Camera2D &camera) noexcept -> void {
        TGX_ASSERT_MSG(camera.zoom != 0.f, "a camera with zoom 0 shows nothing and cannot map back");

        m_camera = camera;
        refit();
    }

    auto Canvas::to_world(Vec2 window_point) const noexcept -> Vec2 {
        const Rect area = covered(m_viewport);
        const Size span = size();
        // A minimized window covers nothing to map from.
        if (!has_area(area) || span.empty()) {
            return m_camera.to_world(window_point);
        }
        const Vec2 canvas_point{
            (window_point.x - area.x) * static_cast<float>(span.width) / area.width,
            (window_point.y - area.y) * static_cast<float>(span.height) / area.height,
        };
        return m_camera.to_world(canvas_point);
    }

    auto Canvas::to_screen(Vec2 world) const noexcept -> Vec2 {
        const Rect area = covered(m_viewport);
        const Size span = size();
        const Vec2 canvas_point = m_camera.to_screen(world);
        if (!has_area(area) || span.empty()) {
            return canvas_point;
        }
        return {
            canvas_point.x * area.width / static_cast<float>(span.width) + area.x,
            canvas_point.y * area.height / static_cast<float>(span.height) + area.y,
        };
    }

    auto Canvas::refit() noexcept -> void {
        m_transform_size = size();
        m_transform = projection_for(m_transform_size) * m_camera.matrix();
    }

    auto Canvas::set_shader(gl::Shader *shader) noexcept -> void {
        m_shader = shader;
        m_u_projection = -1;
        if (!shader) {
            return;
        }

        // The projection is the one uniform the canvas needs: looked up through
        // the shader first to assert it exists and is a mat4.
        (void) shader->uniform<Mat4>("u_projection");
        m_u_projection = detail::projection_location(*shader);

        // The texture is optional: a shader may ignore it. The batch draws
        // without Device::draw's checks, so the one on its samplers is here.
        for (const auto &sampler: gl::detail::samplers(*shader)) {
            TGX_ASSERT_MSG(
                sampler.name == "u_texture",
                "sampler '{}': a canvas shader gets no texture but u_texture",
                sampler.name
            );
            shader->set(shader->uniform<gl::TextureSlot>("u_texture"), {0});
        }
    }

    auto Canvas::clear(Color color) noexcept -> void {
        detail::context().clear({.color = color});
    }

    auto Canvas::flush() noexcept -> void {
        detail::context().flush();
    }

    auto Canvas::state_for(GlId texture) noexcept -> detail::BatchState {
        // A canvas that follows the window notices a resize here, at its first
        // shape after it.
        if (size() != m_transform_size) {
            refit();
        }
        return {
            .texture = texture,
            .blend = m_blend,
            .program = m_shader ? m_shader->id() : 0,
            .u_projection = m_u_projection,
            .transform = m_transform,
            .viewport = pixel_viewport(m_viewport),
        };
    }

    auto Canvas::rect(Rect rect, Color color) noexcept -> void {
        quad({rect.x, rect.y}, {rect.right(), rect.y}, {rect.right(), rect.bottom()}, {rect.x, rect.bottom()}, color);
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

    auto Canvas::triangle(Vec2 a, Vec2 b, Vec2 c, Color color) noexcept -> void {
        auto [batch, first] = start(state_for(0), 3, 3);

        batch.push_vertex({a, white_uv, color});
        batch.push_vertex({b, white_uv, color});
        batch.push_vertex({c, white_uv, color});
        push_indices(batch, first, {0, 1, 2});
    }

    auto Canvas::line(Vec2 a, Vec2 b, Color color, float thickness) noexcept -> void {
        // A point has no direction to widen it across.
        const Vec2 along = b - a;
        if (along == Vec2{}) {
            return;
        }

        const Vec2 side = perpendicular(normalize(along)) * (thickness / 2.f);
        quad(a + side, b + side, b - side, a - side, color);
    }

    auto Canvas::circle(Vec2 center, float radius, Color color) noexcept -> void {
        if (radius <= 0.f) {
            return;
        }

        const std::size_t n = segments_for(radius * std::abs(m_camera.zoom));
        auto [batch, first] = start(state_for(0), n + 1, n * 3);

        // A fan around the center, vertex first.
        batch.push_vertex({center, white_uv, color});
        for (std::size_t i = 0; i < n; ++i) {
            batch.push_vertex({center + rim_point(i, n) * radius, white_uv, color});
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
        const std::size_t n = segments_for(radius * std::abs(m_camera.zoom));
        auto [batch, first] = start(state_for(0), n * 2, n * 6);

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
        const Size texture_size = texture.size();
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

        auto [batch, first] = start(state_for(texture.id()), 4, 6);
        for (std::size_t i = 0; i < 4; ++i) {
            batch.push_vertex({corners[i], uvs[i], sprite.tint});
        }
        // Two triangles: a b c and c d a.
        push_indices(batch, first, {0, 1, 2, 2, 3, 0});
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
            // A space is all advance and no ink.
            if (glyph_index != 0) {
                const float u0 = static_cast<float>(glyph.x) / font::atlas_width;
                const float u1 = static_cast<float>(glyph.x + glyph.width) / font::atlas_width;

                auto [batch, first] = start(state, 4, 6);
                batch.push_vertex({pen, {u0, 0.f}, color});
                batch.push_vertex({pen + Vec2{width, 0.f}, {u1, 0.f}, color});
                batch.push_vertex({pen + Vec2{width, size}, {u1, 1.f}, color});
                batch.push_vertex({pen + Vec2{0.f, size}, {u0, 1.f}, color});
                push_indices(batch, first, {0, 1, 2, 2, 3, 0});
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

    auto Canvas::quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) noexcept -> void {
        auto [batch, first] = start(state_for(0), 4, 6);

        batch.push_vertex({a, white_uv, color});
        batch.push_vertex({b, white_uv, color});
        batch.push_vertex({c, white_uv, color});
        batch.push_vertex({d, white_uv, color});
        // Two triangles: a b c and c d a.
        push_indices(batch, first, {0, 1, 2, 2, 3, 0});
    }
}
