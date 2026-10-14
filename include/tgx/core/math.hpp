#pragma once

#include "tgx/core/assert.hpp"

#include <array>
#include <cmath>
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

    // Axis-aligned: top-left corner and size, y down.
    struct Rect {
        float x{0.f};
        float y{0.f};
        float width{0.f};
        float height{0.f};

        [[nodiscard]] constexpr auto right() const noexcept -> float { return x + width; }

        [[nodiscard]] constexpr auto bottom() const noexcept -> float { return y + height; }

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
    // A point p of the area is at (p - {r.x, r.y}) * content.width / r.width
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

    [[nodiscard]] constexpr auto radians(float degrees) noexcept -> float {
        return degrees * (std::numbers::pi_v<float> / 180.f);
    }

    // Vec2

    [[nodiscard]] constexpr auto operator+(Vec2 a, Vec2 b) noexcept -> Vec2 { return {a.x + b.x, a.y + b.y}; }
    [[nodiscard]] constexpr auto operator-(Vec2 a, Vec2 b) noexcept -> Vec2 { return {a.x - b.x, a.y - b.y}; }
    [[nodiscard]] constexpr auto operator-(Vec2 v) noexcept -> Vec2 { return {-v.x, -v.y}; }
    [[nodiscard]] constexpr auto operator*(Vec2 v, float s) noexcept -> Vec2 { return {v.x * s, v.y * s}; }
    [[nodiscard]] constexpr auto operator*(float s, Vec2 v) noexcept -> Vec2 { return v * s; }
    [[nodiscard]] constexpr auto operator/(Vec2 v, float s) noexcept -> Vec2 { return {v.x / s, v.y / s}; }

    constexpr auto operator+=(Vec2 &a, Vec2 b) noexcept -> Vec2 & { return a = a + b; }
    constexpr auto operator-=(Vec2 &a, Vec2 b) noexcept -> Vec2 & { return a = a - b; }
    constexpr auto operator*=(Vec2 &v, float s) noexcept -> Vec2 & { return v = v * s; }
    constexpr auto operator/=(Vec2 &v, float s) noexcept -> Vec2 & { return v = v / s; }

    [[nodiscard]] constexpr auto dot(Vec2 a, Vec2 b) noexcept -> float { return a.x * b.x + a.y * b.y; }
    // The z of the 3D cross product. Positive when b turns clockwise from a on
    // screen (y down), negative counter-clockwise, zero when parallel.
    [[nodiscard]] constexpr auto cross(Vec2 a, Vec2 b) noexcept -> float { return a.x * b.y - a.y * b.x; }
    // Compare with a squared distance to skip the square root.
    [[nodiscard]] constexpr auto length_squared(Vec2 v) noexcept -> float { return dot(v, v); }

    [[nodiscard]] inline auto length(Vec2 v) noexcept -> float { return std::sqrt(length_squared(v)); }
    // NaNs for a zero vector.
    [[nodiscard]] inline auto normalize(Vec2 v) noexcept -> Vec2 { return v / length(v); }

    [[nodiscard]] inline auto distance(Vec2 a, Vec2 b) noexcept -> float { return length(b - a); }

    // 2D angles are radians from +x, clockwise on screen (y down), like Canvas
    // and Camera2D rotations.

    // The unit vector: from_angle(0) is {1, 0}.
    [[nodiscard]] inline auto from_angle(float angle) noexcept -> Vec2 { return {std::cos(angle), std::sin(angle)}; }

    // In [-pi, pi]; 0 for a zero vector.
    [[nodiscard]] inline auto angle(Vec2 v) noexcept -> float {
        // atan2 gives pi for x = -0; any zero vector gets 0.
        return v == Vec2{} ? 0.f : std::atan2(v.y, v.x);
    }

    // Around {0, 0}.
    [[nodiscard]] inline auto rotate(Vec2 v, float angle) noexcept -> Vec2 {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return {v.x * c - v.y * s, v.x * s + v.y * c};
    }

    // A quarter turn counter-clockwise on screen (to the left of v), like
    // rotate(v, -pi / 2) but exact.
    [[nodiscard]] constexpr auto perpendicular(Vec2 v) noexcept -> Vec2 { return {v.y, -v.x}; }

    // Not clamped: t outside [0, 1] goes past a or b.
    [[nodiscard]] constexpr auto lerp(Vec2 a, Vec2 b, float t) noexcept -> Vec2 { return a + (b - a) * t; }

    // At most max_step closer, never past to. For chasing at a speed:
    // from = move_towards(from, to, speed * dt).
    [[nodiscard]] inline auto move_towards(Vec2 from, Vec2 to, float max_step) noexcept -> Vec2 {
        TGX_ASSERT_MSG(max_step >= 0.f, "move_towards by {}: a step backwards", max_step);

        const Vec2 d = to - from;
        const float dist = length(d);
        return dist <= max_step || dist == 0.f ? to : from + d * (max_step / dist);
    }

    // Vec3

    [[nodiscard]] constexpr auto operator+(Vec3 a, Vec3 b) noexcept -> Vec3 {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    [[nodiscard]] constexpr auto operator-(Vec3 a, Vec3 b) noexcept -> Vec3 {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    [[nodiscard]] constexpr auto operator-(Vec3 v) noexcept -> Vec3 { return {-v.x, -v.y, -v.z}; }
    [[nodiscard]] constexpr auto operator*(Vec3 v, float s) noexcept -> Vec3 { return {v.x * s, v.y * s, v.z * s}; }
    [[nodiscard]] constexpr auto operator*(float s, Vec3 v) noexcept -> Vec3 { return v * s; }
    [[nodiscard]] constexpr auto operator/(Vec3 v, float s) noexcept -> Vec3 { return {v.x / s, v.y / s, v.z / s}; }

    constexpr auto operator+=(Vec3 &a, Vec3 b) noexcept -> Vec3 & { return a = a + b; }
    constexpr auto operator-=(Vec3 &a, Vec3 b) noexcept -> Vec3 & { return a = a - b; }
    constexpr auto operator*=(Vec3 &v, float s) noexcept -> Vec3 & { return v = v * s; }
    constexpr auto operator/=(Vec3 &v, float s) noexcept -> Vec3 & { return v = v / s; }

    [[nodiscard]] constexpr auto dot(Vec3 a, Vec3 b) noexcept -> float { return a.x * b.x + a.y * b.y + a.z * b.z; }

    // Perpendicular to both, by the right-hand rule: cross(x, y) is z.
    [[nodiscard]] constexpr auto cross(Vec3 a, Vec3 b) noexcept -> Vec3 {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }

    [[nodiscard]] constexpr auto length_squared(Vec3 v) noexcept -> float { return dot(v, v); }

    [[nodiscard]] inline auto length(Vec3 v) noexcept -> float { return std::sqrt(length_squared(v)); }
    // NaNs for a zero vector.
    [[nodiscard]] inline auto normalize(Vec3 v) noexcept -> Vec3 { return v / length(v); }

    [[nodiscard]] inline auto distance(Vec3 a, Vec3 b) noexcept -> float { return length(b - a); }

    // Not clamped: t outside [0, 1] goes past a or b.
    [[nodiscard]] constexpr auto lerp(Vec3 a, Vec3 b, float t) noexcept -> Vec3 { return a + (b - a) * t; }

    // At most max_step closer, never past to.
    [[nodiscard]] inline auto move_towards(Vec3 from, Vec3 to, float max_step) noexcept -> Vec3 {
        TGX_ASSERT_MSG(max_step >= 0.f, "move_towards by {}: a step backwards", max_step);

        const Vec3 d = to - from;
        const float dist = length(d);
        return dist <= max_step || dist == 0.f ? to : from + d * (max_step / dist);
    }

    // Vec4

    [[nodiscard]] constexpr auto operator+(Vec4 a, Vec4 b) noexcept -> Vec4 {
        return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
    }

    [[nodiscard]] constexpr auto operator-(Vec4 a, Vec4 b) noexcept -> Vec4 {
        return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
    }

    [[nodiscard]] constexpr auto operator-(Vec4 v) noexcept -> Vec4 { return {-v.x, -v.y, -v.z, -v.w}; }

    [[nodiscard]] constexpr auto operator*(Vec4 v, float s) noexcept -> Vec4 {
        return {v.x * s, v.y * s, v.z * s, v.w * s};
    }

    [[nodiscard]] constexpr auto operator*(float s, Vec4 v) noexcept -> Vec4 { return v * s; }

    [[nodiscard]] constexpr auto operator/(Vec4 v, float s) noexcept -> Vec4 {
        return {v.x / s, v.y / s, v.z / s, v.w / s};
    }

    constexpr auto operator+=(Vec4 &a, Vec4 b) noexcept -> Vec4 & { return a = a + b; }
    constexpr auto operator-=(Vec4 &a, Vec4 b) noexcept -> Vec4 & { return a = a - b; }
    constexpr auto operator*=(Vec4 &v, float s) noexcept -> Vec4 & { return v = v * s; }
    constexpr auto operator/=(Vec4 &v, float s) noexcept -> Vec4 & { return v = v / s; }

    [[nodiscard]] constexpr auto dot(Vec4 a, Vec4 b) noexcept -> float {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    [[nodiscard]] constexpr auto lerp(Vec4 a, Vec4 b, float t) noexcept -> Vec4 { return a + (b - a) * t; }

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
        Mat4 m;
        m.columns[3] = {offset.x, offset.y, offset.z, 1.f};
        return m;
    }

    [[nodiscard]] constexpr auto scale(Vec3 factors) noexcept -> Mat4 {
        Mat4 m;
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

    // Easing

    // Curves that bend progress t (0 to 1) so motion starts or ends softly,
    // for lerp() or std::lerp:
    //
    //     pos = tgx::lerp(from, to, tgx::ease::out_cubic(timer.progress()));
    //
    // in: slow start, out: slow end, in_out: both. t is clamped to [0, 1].
    // Every curve gives 0 at 0 and 1 at 1; back and elastic overshoot in
    // between, by design.
    namespace ease {
        namespace detail {
            [[nodiscard]] constexpr auto unit(float t) noexcept -> float { return t < 0.f ? 0.f : t > 1.f ? 1.f : t; }

            // For the curves that are not constexpr anyway: a NaN would
            // pass the clamp.
            [[nodiscard]] inline auto checked_unit(float t) noexcept -> float {
                TGX_ASSERT_MSG(!std::isnan(t), "easing a NaN");
                return unit(t);
            }

            constexpr float back = 1.70158f;
            constexpr float back_in_out = back * 1.525f;

            [[nodiscard]] constexpr auto out_bounce(float t) noexcept -> float {
                constexpr float n = 7.5625f;
                constexpr float d = 2.75f;
                if (t < 1.f / d) {
                    return n * t * t;
                }
                if (t < 2.f / d) {
                    t -= 1.5f / d;
                    return n * t * t + 0.75f;
                }
                if (t < 2.5f / d) {
                    t -= 2.25f / d;
                    return n * t * t + 0.9375f;
                }
                t -= 2.625f / d;
                return n * t * t + 0.984375f;
            }
        }

        [[nodiscard]] constexpr auto linear(float t) noexcept -> float { return detail::unit(t); }

        // Sine: the softest.
        [[nodiscard]] inline auto in_sine(float t) noexcept -> float {
            t = detail::checked_unit(t);
            return 1.f - std::cos(t * std::numbers::pi_v<float> / 2.f);
        }

        [[nodiscard]] inline auto out_sine(float t) noexcept -> float {
            t = detail::checked_unit(t);
            return std::sin(t * std::numbers::pi_v<float> / 2.f);
        }

        [[nodiscard]] inline auto in_out_sine(float t) noexcept -> float {
            t = detail::checked_unit(t);
            return (1.f - std::cos(t * std::numbers::pi_v<float>)) / 2.f;
        }

        // Quad: t squared.
        [[nodiscard]] constexpr auto in_quad(float t) noexcept -> float {
            t = detail::unit(t);
            return t * t;
        }

        [[nodiscard]] constexpr auto out_quad(float t) noexcept -> float {
            t = 1.f - detail::unit(t);
            return 1.f - t * t;
        }

        [[nodiscard]] constexpr auto in_out_quad(float t) noexcept -> float {
            t = detail::unit(t);
            const float u = 2.f - 2.f * t;
            return t < 0.5f ? 2.f * t * t : 1.f - u * u / 2.f;
        }

        // Cubic: t cubed, the usual choice.
        [[nodiscard]] constexpr auto in_cubic(float t) noexcept -> float {
            t = detail::unit(t);
            return t * t * t;
        }

        [[nodiscard]] constexpr auto out_cubic(float t) noexcept -> float {
            t = 1.f - detail::unit(t);
            return 1.f - t * t * t;
        }

        [[nodiscard]] constexpr auto in_out_cubic(float t) noexcept -> float {
            t = detail::unit(t);
            const float u = 2.f - 2.f * t;
            return t < 0.5f ? 4.f * t * t * t : 1.f - u * u * u / 2.f;
        }

        // Expo: the sharpest.
        [[nodiscard]] inline auto in_expo(float t) noexcept -> float {
            t = detail::checked_unit(t);
            return t == 0.f ? 0.f : std::exp2(10.f * t - 10.f);
        }

        [[nodiscard]] inline auto out_expo(float t) noexcept -> float {
            t = detail::checked_unit(t);
            return t == 1.f ? 1.f : 1.f - std::exp2(-10.f * t);
        }

        [[nodiscard]] inline auto in_out_expo(float t) noexcept -> float {
            t = detail::checked_unit(t);
            if (t == 0.f || t == 1.f) {
                return t;
            }
            return t < 0.5f ? std::exp2(20.f * t - 10.f) / 2.f : (2.f - std::exp2(10.f - 20.f * t)) / 2.f;
        }

        // Back: pulls back below 0 (in) or overshoots past 1 (out) first.
        [[nodiscard]] constexpr auto in_back(float t) noexcept -> float {
            t = detail::unit(t);
            return (detail::back + 1.f) * t * t * t - detail::back * t * t;
        }

        [[nodiscard]] constexpr auto out_back(float t) noexcept -> float {
            t = detail::unit(t) - 1.f;
            return 1.f + (detail::back + 1.f) * t * t * t + detail::back * t * t;
        }

        [[nodiscard]] constexpr auto in_out_back(float t) noexcept -> float {
            t = detail::unit(t);
            constexpr float c = detail::back_in_out;
            if (t < 0.5f) {
                const float u = 2.f * t;
                return u * u * ((c + 1.f) * u - c) / 2.f;
            }
            const float u = 2.f * t - 2.f;
            return (u * u * ((c + 1.f) * u + c) + 2.f) / 2.f;
        }

        // Elastic: swings around the end like a spring.
        [[nodiscard]] inline auto in_elastic(float t) noexcept -> float {
            t = detail::checked_unit(t);
            if (t == 0.f || t == 1.f) {
                return t;
            }
            constexpr float c = 2.f * std::numbers::pi_v<float> / 3.f;
            return -std::exp2(10.f * t - 10.f) * std::sin((10.f * t - 10.75f) * c);
        }

        [[nodiscard]] inline auto out_elastic(float t) noexcept -> float {
            t = detail::checked_unit(t);
            if (t == 0.f || t == 1.f) {
                return t;
            }
            constexpr float c = 2.f * std::numbers::pi_v<float> / 3.f;
            return std::exp2(-10.f * t) * std::sin((10.f * t - 0.75f) * c) + 1.f;
        }

        [[nodiscard]] inline auto in_out_elastic(float t) noexcept -> float {
            t = detail::checked_unit(t);
            if (t == 0.f || t == 1.f) {
                return t;
            }
            constexpr float c = 2.f * std::numbers::pi_v<float> / 4.5f;
            const float swing = std::sin((20.f * t - 11.125f) * c);
            if (t < 0.5f) {
                return -std::exp2(20.f * t - 10.f) * swing / 2.f;
            }
            return std::exp2(10.f - 20.f * t) * swing / 2.f + 1.f;
        }

        // Bounce: lands like a ball (out), or takes off like one (in).
        [[nodiscard]] constexpr auto in_bounce(float t) noexcept -> float {
            return 1.f - detail::out_bounce(1.f - detail::unit(t));
        }

        [[nodiscard]] constexpr auto out_bounce(float t) noexcept -> float {
            return detail::out_bounce(detail::unit(t));
        }

        [[nodiscard]] constexpr auto in_out_bounce(float t) noexcept -> float {
            t = detail::unit(t);
            if (t < 0.5f) {
                return (1.f - detail::out_bounce(1.f - 2.f * t)) / 2.f;
            }
            return (1.f + detail::out_bounce(2.f * t - 1.f)) / 2.f;
        }
    }
}
