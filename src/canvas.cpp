#include "tgx/canvas.h"

#include "tgx/assert.h"
#include "tgx/texture.h"

#include "tgx/gl/device.h"
#include "tgx/gl/handle.h"
#include "tgx/gl/shader.h"

#include "batch.h"
#include "device_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <numbers>
#include <string_view>
#include <utility>

namespace {
    using tgx::gl::detail::Batch;
    using tgx::gl::detail::BatchState;
    using tgx::gl::detail::BatchVertex;

    // Segments for a circle: enough that no edge strays more than a quarter
    // unit from the true circle, as large as it shows on screen.
    constexpr float circle_tolerance = 0.25f;
    constexpr std::size_t min_segments = 8;
    constexpr std::size_t max_segments = 1024;

    static_assert(
        max_segments * 2 <= tgx::gl::detail::batch_max_vertices
        && max_segments * 6 <= tgx::gl::detail::batch_max_indices
    );

    // Where shapes sample the white texture: any point of a 1x1 one will do.
    constexpr tgx::Vec2 white_uv{0.5f, 0.5f};

    // The canvas spans the size with y down: (0, 0) at the top-left. An empty
    // size (a minimized window) would divide by zero; nothing shows then anyway.
    [[nodiscard]] constexpr auto projection_for(tgx::Size size) noexcept -> tgx::Mat4 {
        if (size.empty()) {
            return {};
        }
        return tgx::ortho(0.f, static_cast<float>(size.width), static_cast<float>(size.height), 0.f);
    }

    // Rotated a quarter turn counter-clockwise on screen (y down).
    [[nodiscard]] constexpr auto perpendicular(tgx::Vec2 v) noexcept -> tgx::Vec2 {
        return {v.y, -v.x};
    }

    [[nodiscard]] auto segments_for(float screen_radius) noexcept -> std::size_t {
        if (screen_radius <= circle_tolerance) {
            return min_segments;
        }
        // An edge of a circle split into n strays r * (1 - cos(pi / n)) from it.
        const float n = std::numbers::pi_v<float> / std::acos(1.f - circle_tolerance / screen_radius);
        return std::clamp(static_cast<std::size_t>(std::ceil(n)), min_segments, max_segments);
    }

    // The unit circle split into n, from angle 0 clockwise on screen.
    [[nodiscard]] auto rim_point(std::size_t i, std::size_t n) noexcept -> tgx::Vec2 {
        const float angle = 2.f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(n);
        return {std::cos(angle), std::sin(angle)};
    }

    // The Device's batch, with room for this many more vertices and indices
    // drawn with the state, and the index the first of them gets.
    [[nodiscard]] auto start(
        const BatchState &state,
        std::size_t vertex_count,
        std::size_t index_count
    ) noexcept -> std::pair<Batch &, std::uint16_t> {
        tgx::gl::Device &device = tgx::detail::device();
        Batch &batch = tgx::gl::detail::DeviceAccess::batch(device);
        const std::uint16_t first = batch.reserve(device, state, vertex_count, index_count);
        return {batch, first};
    }

    auto push_indices(Batch &batch, std::uint16_t first, std::initializer_list<std::size_t> offsets) noexcept -> void {
        for (const std::size_t offset : offsets) {
            batch.push_index(static_cast<std::uint16_t>(first + offset));
        }
    }
}

namespace tgx {
    auto Canvas::create(Size size) noexcept -> Canvas {
        TGX_ASSERT_MSG(gl::detail::context_alive(), "creating a Canvas before the Device");

        return Canvas{size};
    }

    Canvas::Canvas(Size size) noexcept : m_size{size}, m_transform{projection_for(size)} {
    }

    auto Canvas::set_size(Size size) noexcept -> void {
        m_size = size;
        m_transform = projection_for(m_size) * m_camera.matrix();
    }

    auto Canvas::set_camera(const Camera2D &camera) noexcept -> void {
        TGX_ASSERT_MSG(camera.zoom != 0.f, "a camera with zoom 0 shows nothing and cannot map back");

        m_camera = camera;
        m_transform = projection_for(m_size) * m_camera.matrix();
    }

    auto Canvas::set_shader(gl::Shader *shader) noexcept -> void {
        m_shader = shader;
        if (shader == nullptr) {
            return;
        }

        // The projection is the one uniform the canvas needs: looked up here
        // to assert it exists. The texture is optional: a shader may ignore it.
        (void) shader->uniform<Mat4>("u_projection");
        const auto samplers = gl::detail::samplers(*shader);
        if (std::ranges::contains(samplers, std::string_view{"u_texture"}, &gl::detail::ShaderSampler::name)) {
            shader->set(shader->uniform<gl::TextureSlot>("u_texture"), {0});
        }
    }

    auto Canvas::clear(Color color) noexcept -> void {
        detail::device().clear({.color = color});
    }

    auto Canvas::flush() noexcept -> void {
        detail::device().flush();
    }

    auto Canvas::state_for(const Texture *texture) const noexcept -> gl::detail::BatchState {
        return {.texture = texture, .blend = m_blend, .shader = m_shader, .transform = m_transform};
    }

    auto Canvas::rect(Rect rect, Color color) noexcept -> void {
        const float left = rect.x;
        const float top = rect.y;
        const float right = rect.x + rect.width;
        const float bottom = rect.y + rect.height;
        quad({left, top}, {right, top}, {right, bottom}, {left, bottom}, color);
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
        this->rect({rect.x, rect.y + rect.height - t, rect.width, t}, color);
        this->rect({rect.x, rect.y + t, t, rect.height - 2.f * t}, color);
        this->rect({rect.x + rect.width - t, rect.y + t, t, rect.height - 2.f * t}, color);
    }

    auto Canvas::triangle(Vec2 a, Vec2 b, Vec2 c, Color color) noexcept -> void {
        auto [batch, first] = start(state_for(nullptr), 3, 3);

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
        auto [batch, first] = start(state_for(nullptr), n + 1, n * 3);

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
        auto [batch, first] = start(state_for(nullptr), n * 2, n * 6);

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
            for (Vec2 &corner : corners) {
                corner += sprite.position;
            }
        } else {
            const float c = std::cos(sprite.rotation);
            const float s = std::sin(sprite.rotation);
            for (Vec2 &corner : corners) {
                corner = Vec2{corner.x * c - corner.y * s, corner.x * s + corner.y * c} + sprite.position;
            }
        }

        auto [batch, first] = start(state_for(&texture), 4, 6);
        for (std::size_t i = 0; i < 4; ++i) {
            batch.push_vertex({corners[i], uvs[i], sprite.tint});
        }
        // Two triangles: a b c and c d a.
        push_indices(batch, first, {0, 1, 2, 2, 3, 0});
    }

    auto Canvas::quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) noexcept -> void {
        auto [batch, first] = start(state_for(nullptr), 4, 6);

        batch.push_vertex({a, white_uv, color});
        batch.push_vertex({b, white_uv, color});
        batch.push_vertex({c, white_uv, color});
        batch.push_vertex({d, white_uv, color});
        // Two triangles: a b c and c d a.
        push_indices(batch, first, {0, 1, 2, 2, 3, 0});
    }
}
