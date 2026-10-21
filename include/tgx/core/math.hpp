#pragma once

#include "tgx/core/assert.hpp"

#include <array>
#include <cmath>
#include <concepts>
#include <functional>
#include <numbers>
#include <type_traits>

namespace tgx {
    // Vectors and a matrix with the memory layout of GLSL and glm: they go to
    // the GPU byte for byte and convert to glm with std::bit_cast. glm's
    // conventions too: right-handed, depth -1..1, radians.

    struct Vec2 {
        float x{0.f};
        float y{0.f};

        [[nodiscard]] constexpr auto operator==(const Vec2 &) const noexcept -> bool = default;
    };

    struct Vec3 {
        float x{0.f};
        float y{0.f};
        float z{0.f};

        [[nodiscard]] constexpr auto operator==(const Vec3 &) const noexcept -> bool = default;
    };

    struct Vec4 {
        float x{0.f};
        float y{0.f};
        float z{0.f};
        float w{0.f};

        [[nodiscard]] constexpr auto operator==(const Vec4 &) const noexcept -> bool = default;
    };

    // Pixels for images, textures and framebuffers, screen coordinates for
    // the window, canvas units for a Canvas. Signed, like GLFW and GL.
    struct Size {
        int width{0};
        int height{0};

        // E.g. the framebuffer of a minimized window.
        [[nodiscard]] constexpr auto empty() const noexcept -> bool { return width <= 0 || height <= 0; }

        // Width / height. 1 when empty, so a minimized window puts no inf or
        // NaN into a projection.
        [[nodiscard]] constexpr auto aspect() const noexcept -> float {
            return empty() ? 1.f : static_cast<float>(width) / static_cast<float>(height);
        }

        [[nodiscard]] constexpr auto operator==(const Size &) const noexcept -> bool = default;
    };

    // For math with positions: to_vec2(canvas.size()) / 2 is the center.
    [[nodiscard]] constexpr auto to_vec2(Size size) noexcept -> Vec2 {
        return {static_cast<float>(size.width), static_cast<float>(size.height)};
    }

    // Axis-aligned: top-left corner and size, y down.
    struct Rect {
        float x{0.f};
        float y{0.f};
        float width{0.f};
        float height{0.f};

        [[nodiscard]] constexpr auto right() const noexcept -> float { return x + width; }

        [[nodiscard]] constexpr auto bottom() const noexcept -> float { return y + height; }

        // The top-left corner.
        [[nodiscard]] constexpr auto position() const noexcept -> Vec2 { return {x, y}; }

        [[nodiscard]] constexpr auto size() const noexcept -> Vec2 { return {width, height}; }

        [[nodiscard]] constexpr auto center() const noexcept -> Vec2 { return {x + width / 2.f, y + height / 2.f}; }

        // A button around its label, a sprite around its position.
        [[nodiscard]] static constexpr auto from_center(Vec2 center, Vec2 size) noexcept -> Rect {
            return {center.x - size.x / 2.f, center.y - size.y / 2.f, size.x, size.y};
        }

        // A side is 0 or less.
        [[nodiscard]] constexpr auto empty() const noexcept -> bool { return width <= 0.f || height <= 0.f; }

        [[nodiscard]] constexpr auto operator==(const Rect &) const noexcept -> bool = default;
    };

    // Column-major, like GLSL's mat4: a translation is in columns[3]. Starts as
    // the identity, so an unset matrix does not collapse things to a point.
    struct Mat4 {
        std::array<Vec4, 4> columns{
            {
                {1.f, 0.f, 0.f, 0.f},
                {0.f, 1.f, 0.f, 0.f},
                {0.f, 0.f, 1.f, 0.f},
                {0.f, 0.f, 0.f, 1.f},
            }
        };

        [[nodiscard]] constexpr auto operator==(const Mat4 &) const noexcept -> bool = default;
    };

    static_assert(sizeof(Vec2) == 2 * sizeof(float));
    static_assert(sizeof(Vec3) == 3 * sizeof(float));
    static_assert(sizeof(Vec4) == 4 * sizeof(float));
    static_assert(sizeof(Mat4) == 16 * sizeof(float));

    static_assert(std::is_standard_layout_v<Vec2> && std::is_trivially_copyable_v<Vec2>);
    static_assert(std::is_standard_layout_v<Vec3> && std::is_trivially_copyable_v<Vec3>);
    static_assert(std::is_standard_layout_v<Vec4> && std::is_trivially_copyable_v<Vec4>);
    static_assert(std::is_standard_layout_v<Mat4> && std::is_trivially_copyable_v<Mat4>);
    static_assert(std::is_standard_layout_v<Rect> && std::is_trivially_copyable_v<Rect>);

    // Where content goes inside area: as large as fits with proportions kept,
    // centered, bars on two sides. Empty when either size is empty.
    //
    // A point p of the area is at (p - r.position()) * content.width / r.width
    // (and the same for y) in the content.
    [[nodiscard]] constexpr auto fit(Size content, Size area) noexcept -> Rect {
        if (content.empty() || area.empty()) {
            return {};
        }
        const float sx = static_cast<float>(area.width) / static_cast<float>(content.width);
        const float sy = static_cast<float>(area.height) / static_cast<float>(content.height);
        const float scale = sx < sy ? sx : sy;
        const float w = static_cast<float>(content.width) * scale;
        const float h = static_cast<float>(content.height) * scale;
        return {(static_cast<float>(area.width) - w) / 2.f, (static_cast<float>(area.height) - h) / 2.f, w, h};
    }

    // Like fit(), but scaled by a whole factor (at least 1) at a whole
    // position, so pixel art keeps square pixels: 320x180 in 1366x768 is
    // 1280x720 at (43, 24). Area units are pixels only at a whole display
    // scale; at 1.25, fit into window().framebuffer_size() and set_size()
    // the canvas to it.
    [[nodiscard]] constexpr auto fit_whole(Size content, Size area) noexcept -> Rect {
        if (content.empty() || area.empty()) {
            return {};
        }
        const int sx = area.width / content.width;
        const int sy = area.height / content.height;
        const int scale = sx < sy ? (sx > 1 ? sx : 1) : (sy > 1 ? sy : 1);
        const int w = content.width * scale;
        const int h = content.height * scale;
        return {
            static_cast<float>((area.width - w) / 2),
            static_cast<float>((area.height - h) / 2),
            static_cast<float>(w),
            static_cast<float>(h),
        };
    }

    // Frame index of a sprite sheet, a grid of cells counted left to right,
    // then top to bottom: the Sprite::src to draw it. With one animation per
    // row, pass row * columns + frame.
    //
    //     canvas.sprite(sheet, {.position = pos, .src = tgx::frame_of(sheet.size(), {16, 16}, frame)});
    [[nodiscard]] constexpr auto frame_of(Size sheet, Size cell, int index) noexcept -> Rect {
        TGX_ASSERT_MSG(!cell.empty(), "frames of {}x{}", cell.width, cell.height);
        const int columns = sheet.width / cell.width;
        const int rows = sheet.height / cell.height;
        TGX_ASSERT_MSG(
            index >= 0 && index < columns * rows,
            "frame {} of a {}x{} sheet of {}x{} frames",
            index, sheet.width, sheet.height, cell.width, cell.height
        );
        return {
            static_cast<float>(index % columns * cell.width),
            static_cast<float>(index / columns * cell.height),
            static_cast<float>(cell.width),
            static_cast<float>(cell.height),
        };
    }

    [[nodiscard]] constexpr auto radians(float degrees) noexcept -> float {
        return degrees * (std::numbers::pi_v<float> / 180.f);
    }

    // value at the same place between to_start and to_end as it is between
    // from_start and from_end, which must differ. Not clamped, like lerp.
    //
    //     const float pan = tgx::remap(x, 0, field_width, -1, 1);   // left edge -1, right edge 1
    [[nodiscard]] constexpr auto remap(
        float value,
        float from_start, float from_end,
        float to_start, float to_end
    ) noexcept -> float {
        return to_start + (value - from_start) / (from_end - from_start) * (to_end - to_start);
    }

    // 0 up to from, 1 from to on, an S-curve between that starts and ends
    // flat. from and to must differ; to below from runs it backwards. As
    // GLSL's smoothstep.
    //
    //     const float day = tgx::smoothstep(-0.1f, 0.3f, sun_height);   // dawn to full light
    [[nodiscard]] constexpr auto smoothstep(float from, float to, float x) noexcept -> float {
        const float t = (x - from) / (to - from);
        const float u = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
        return u * u * (3.f - 2.f * u);
    }

    // Vec2, Vec3, Vec4: what is the same for every size is written once

    template<typename V>
    concept Vector = std::same_as<V, Vec2> || std::same_as<V, Vec3> || std::same_as<V, Vec4>;

    namespace detail {
        // Field by field, like the code written out per size: a version with
        // loops over bit_cast arrays broke rotated shapes in GCC 15 Release.

        template<Vector V, typename F>
        constexpr auto zip(V a, V b, F f) noexcept -> V {
            if constexpr (std::same_as<V, Vec2>) {
                return {f(a.x, b.x), f(a.y, b.y)};
            } else if constexpr (std::same_as<V, Vec3>) {
                return {f(a.x, b.x), f(a.y, b.y), f(a.z, b.z)};
            } else {
                return {f(a.x, b.x), f(a.y, b.y), f(a.z, b.z), f(a.w, b.w)};
            }
        }

        template<Vector V, typename F>
        constexpr auto map(V v, F f) noexcept -> V {
            return zip(v, v, [f](float x, float) { return f(x); });
        }

        template<Vector V>
        constexpr auto sum(V v) noexcept -> float {
            if constexpr (std::same_as<V, Vec2>) {
                return v.x + v.y;
            } else if constexpr (std::same_as<V, Vec3>) {
                return v.x + v.y + v.z;
            } else {
                return v.x + v.y + v.z + v.w;
            }
        }
    }

    template<Vector V>
    [[nodiscard]] constexpr auto operator+(V a, V b) noexcept -> V { return detail::zip(a, b, std::plus{}); }

    template<Vector V>
    [[nodiscard]] constexpr auto operator-(V a, V b) noexcept -> V { return detail::zip(a, b, std::minus{}); }

    template<Vector V>
    [[nodiscard]] constexpr auto operator-(V v) noexcept -> V { return detail::map(v, std::negate{}); }

    template<Vector V>
    [[nodiscard]] constexpr auto operator*(V v, float s) noexcept -> V {
        return detail::map(v, [s](float x) { return x * s; });
    }

    template<Vector V>
    [[nodiscard]] constexpr auto operator*(float s, V v) noexcept -> V { return v * s; }

    template<Vector V>
    [[nodiscard]] constexpr auto operator/(V v, float s) noexcept -> V {
        return detail::map(v, [s](float x) { return x / s; });
    }

    template<Vector V>
    constexpr auto operator+=(V &a, V b) noexcept -> V & { return a = a + b; }

    template<Vector V>
    constexpr auto operator-=(V &a, V b) noexcept -> V & { return a = a - b; }

    template<Vector V>
    constexpr auto operator*=(V &v, float s) noexcept -> V & { return v = v * s; }

    template<Vector V>
    constexpr auto operator/=(V &v, float s) noexcept -> V & { return v = v / s; }

    // The second vector of a pair is not deduced, so lerp(a, {1, 2}, t) works.

    template<Vector V>
    [[nodiscard]] constexpr auto dot(V a, std::type_identity_t<V> b) noexcept -> float {
        return detail::sum(detail::zip(a, b, std::multiplies{}));
    }

    // Compare with a squared distance to skip the square root.
    template<Vector V>
    [[nodiscard]] constexpr auto length_squared(V v) noexcept -> float { return dot(v, v); }

    template<Vector V>
    [[nodiscard]] auto length(V v) noexcept -> float { return std::sqrt(length_squared(v)); }

    // NaNs for a zero vector.
    template<Vector V>
    [[nodiscard]] auto normalize(V v) noexcept -> V { return v / length(v); }

    template<Vector V>
    [[nodiscard]] auto distance(V a, std::type_identity_t<V> b) noexcept -> float { return length(b - a); }

    // Not clamped: t outside [0, 1] goes past a or b.
    template<Vector V>
    [[nodiscard]] constexpr auto lerp(V a, std::type_identity_t<V> b, float t) noexcept -> V {
        return a + (b - a) * t;
    }

    // At most max_step closer, never past to. For chasing at a speed:
    // from = move_towards(from, to, speed * dt).
    template<Vector V>
    [[nodiscard]] auto move_towards(V from, std::type_identity_t<V> to, float max_step) noexcept -> V {
        TGX_ASSERT_MSG(max_step >= 0.f, "move_towards by {}: a step backwards", max_step);

        const V d = to - from;
        const float dist = length(d);
        return dist <= max_step || dist == 0.f ? to : from + d * (max_step / dist);
    }

    // Vec2

    // The z of the 3D cross product. Positive when b turns clockwise from a on
    // screen (y down), negative counter-clockwise, zero when parallel.
    [[nodiscard]] constexpr auto cross(Vec2 a, Vec2 b) noexcept -> float { return a.x * b.y - a.y * b.x; }

    // 2D angles are radians from +x, clockwise on screen (y down), like Canvas
    // and Camera2D rotations.

    // The unit vector: from_angle(0) is {1, 0}.
    [[nodiscard]] inline auto from_angle(float angle) noexcept -> Vec2 { return {std::cos(angle), std::sin(angle)}; }

    // In [-pi, pi]; 0 for a zero vector.
    [[nodiscard]] inline auto angle(Vec2 v) noexcept -> float {
        // atan2 gives pi for x = -0; any zero vector gets 0.
        return v == Vec2{} ? 0.f : std::atan2(v.y, v.x);
    }

    namespace detail {
        // rotate() by the angle whose cosine and sine are turn: for many
        // points, from_angle once.
        [[nodiscard]] constexpr auto turned(Vec2 v, Vec2 turn) noexcept -> Vec2 {
            return {v.x * turn.x - v.y * turn.y, v.x * turn.y + v.y * turn.x};
        }
    }

    // Around {0, 0}.
    [[nodiscard]] inline auto rotate(Vec2 v, float angle) noexcept -> Vec2 {
        return detail::turned(v, from_angle(angle));
    }

    // A quarter turn counter-clockwise on screen (to the left of v), like
    // rotate(v, -pi / 2) but exact.
    [[nodiscard]] constexpr auto perpendicular(Vec2 v) noexcept -> Vec2 { return {v.y, -v.x}; }

    // Vec3

    // Perpendicular to both, by the right-hand rule: cross(x, y) is z.
    [[nodiscard]] constexpr auto cross(Vec3 a, Vec3 b) noexcept -> Vec3 {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }

    // Mat4

    [[nodiscard]] constexpr auto operator*(const Mat4 &m, Vec4 v) noexcept -> Vec4 {
        const auto &c = m.columns;
        return c[0] * v.x + c[1] * v.y + c[2] * v.z + c[3] * v.w;
    }

    // Applies b first, then a, as in GLSL: projection * view * model.
    [[nodiscard]] constexpr auto operator*(const Mat4 &a, const Mat4 &b) noexcept -> Mat4 {
        return {
            {
                {
                    a * b.columns[0],
                    a * b.columns[1],
                    a * b.columns[2],
                    a * b.columns[3],
                }
            }
        };
    }

    [[nodiscard]] constexpr auto translate(Vec3 offset) noexcept -> Mat4 {
        Mat4 m{};
        m.columns[3] = {offset.x, offset.y, offset.z, 1.f};
        return m;
    }

    [[nodiscard]] constexpr auto scale(Vec3 factors) noexcept -> Mat4 {
        Mat4 m{};
        m.columns[0].x = factors.x;
        m.columns[1].y = factors.y;
        m.columns[2].z = factors.z;
        return m;
    }

    // Counter-clockwise when the axis points at the viewer. The axis must be
    // non-zero, not necessarily unit. It comes first, like in rotate(Vec2).
    [[nodiscard]] inline auto rotate(Vec3 axis, float angle) noexcept -> Mat4 {
        const Vec3 a = normalize(axis);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const Vec3 t = a * (1.f - c);

        return {
            {
                {
                    {c + t.x * a.x, t.x * a.y + s * a.z, t.x * a.z - s * a.y, 0.f},
                    {t.y * a.x - s * a.z, c + t.y * a.y, t.y * a.z + s * a.x, 0.f},
                    {t.z * a.x + s * a.y, t.z * a.y - s * a.x, c + t.z * a.z, 0.f},
                    {0.f, 0.f, 0.f, 1.f},
                }
            }
        };
    }

    // Maps [left, right] x [bottom, top] x [-z_near, -z_far] to clip space,
    // without perspective. Same as glm::ortho; z = 0 stays 0 by default. The
    // bounds of each pair must differ. Not near/far: <windows.h> defines
    // those as macros.
    [[nodiscard]] constexpr auto ortho(
        float left, float right,
        float bottom, float top,
        float z_near = -1.f, float z_far = 1.f
    ) noexcept -> Mat4 {
        return {
            {
                {
                    {2.f / (right - left), 0.f, 0.f, 0.f},
                    {0.f, 2.f / (top - bottom), 0.f, 0.f},
                    {0.f, 0.f, -2.f / (z_far - z_near), 0.f},
                    {
                        -(right + left) / (right - left),
                        -(top + bottom) / (top - bottom),
                        -(z_far + z_near) / (z_far - z_near),
                        1.f,
                    },
                }
            }
        };
    }

    // Looks down -z; fov_y is vertical, in radians. 0 < z_near < z_far. Most
    // depth precision is near z_near, so keep it as large as the scene allows.
    // Same as glm::perspective.
    [[nodiscard]] inline auto perspective(
        float fov_y, float aspect,
        float z_near, float z_far
    ) noexcept -> Mat4 {
        const float f = 1.f / std::tan(fov_y / 2.f);
        const float depth = z_far - z_near;

        return {
            {
                {
                    {f / aspect, 0.f, 0.f, 0.f},
                    {0.f, f, 0.f, 0.f},
                    {0.f, 0.f, -(z_far + z_near) / depth, -1.f},
                    {0.f, 0.f, -2.f * z_far * z_near / depth, 0.f},
                }
            }
        };
    }

    // A camera at eye looking at target. up is a rough up direction, not along
    // the view. Same as glm::lookAt.
    [[nodiscard]] inline auto look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept -> Mat4 {
        const Vec3 f = normalize(target - eye);
        const Vec3 s = normalize(cross(f, up));
        const Vec3 u = cross(s, f);

        return {
            {
                {
                    {s.x, u.x, -f.x, 0.f},
                    {s.y, u.y, -f.y, 0.f},
                    {s.z, u.z, -f.z, 0.f},
                    {-dot(s, eye), -dot(u, eye), dot(f, eye), 1.f},
                }
            }
        };
    }
}
