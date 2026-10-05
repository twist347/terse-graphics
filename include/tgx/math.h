#pragma once

#include <array>
#include <cmath>
#include <numbers>
#include <type_traits>

namespace tgx {
    // Plain float vectors and a matrix, laid out as GLSL and glm lay them out:
    // they go to the GPU byte for byte, and glm values convert with std::bit_cast.
    // The operations follow glm's conventions too: right-handed, depth -1..1,
    // angles in radians. Only what the library and its examples need so far.
    // Next to them, the sizes and rectangles windows and 2D drawing measure in.

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

    // Width and height in pixels. Signed, like the windowing and GL APIs these
    // come from and go to.
    struct Size {
        int width{0};
        int height{0};

        // Nothing to draw into, e.g. the framebuffer of a minimized window.
        [[nodiscard]] constexpr auto empty() const noexcept -> bool { return width <= 0 || height <= 0; }

        // Width over height. 1 for an empty size, so a minimized window does not
        // put inf or NaN into a projection.
        [[nodiscard]] constexpr auto aspect() const noexcept -> float {
            return empty() ? 1.f : static_cast<float>(width) / static_cast<float>(height);
        }

        [[nodiscard]] constexpr auto operator==(const Size &) const noexcept -> bool = default;
    };

    // An axis-aligned rectangle: its top-left corner and size, in 2D drawing's
    // y-down coordinates.
    struct Rect {
        float x{0.f};
        float y{0.f};
        float width{0.f};
        float height{0.f};

        [[nodiscard]] constexpr auto right() const noexcept -> float { return x + width; }

        [[nodiscard]] constexpr auto bottom() const noexcept -> float { return y + height; }

        [[nodiscard]] constexpr auto center() const noexcept -> Vec2 { return {x + width / 2.f, y + height / 2.f}; }

        [[nodiscard]] constexpr auto operator==(const Rect &) const noexcept -> bool = default;
    };

    // Column-major, like GLSL's mat4: columns[c] is column c, so a translation
    // sits in columns[3]. Starts as the identity, so a matrix left unset draws
    // untransformed rather than collapsing everything to a point.
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
    // The z of the 3D cross product: positive when b turns clockwise from a on
    // y-down screen coordinates, negative counter-clockwise, zero when they are
    // parallel.
    [[nodiscard]] constexpr auto cross(Vec2 a, Vec2 b) noexcept -> float { return a.x * b.y - a.y * b.x; }
    [[nodiscard]] inline auto length(Vec2 v) noexcept -> float { return std::sqrt(dot(v, v)); }
    // A zero vector has no direction; normalizing one gives NaNs.
    [[nodiscard]] inline auto normalize(Vec2 v) noexcept -> Vec2 { return v / length(v); }

    [[nodiscard]] inline auto distance(Vec2 a, Vec2 b) noexcept -> float { return length(b - a); }

    // Angles of 2D vectors are in radians from +x, clockwise on screen (y
    // down), as Canvas rotations and Camera2D turn.

    // The unit vector at the angle: from_angle(0) is {1, 0}.
    [[nodiscard]] inline auto from_angle(float angle) noexcept -> Vec2 { return {std::cos(angle), std::sin(angle)}; }

    // The angle of the vector, in [-pi, pi]; 0 for a zero vector.
    [[nodiscard]] inline auto angle(Vec2 v) noexcept -> float {
        // atan2 tells the zeros apart: -0 in x gives pi. A zero vector, of
        // either sign, has no direction.
        return v == Vec2{} ? 0.f : std::atan2(v.y, v.x);
    }

    // Turned by angle around {0, 0}.
    [[nodiscard]] inline auto rotate(Vec2 v, float angle) noexcept -> Vec2 {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return {v.x * c - v.y * s, v.x * s + v.y * c};
    }

    // Turned a quarter counter-clockwise on screen, as rotate(v, -pi / 2)
    // but exact: the left of v when facing along it.
    [[nodiscard]] constexpr auto perpendicular(Vec2 v) noexcept -> Vec2 { return {v.y, -v.x}; }

    // a at t = 0, b at t = 1, on along the line beyond them.
    [[nodiscard]] constexpr auto lerp(Vec2 a, Vec2 b, float t) noexcept -> Vec2 { return a + (b - a) * t; }

    // From towards to by max_step at most, landing on to rather than past it:
    // chasing a target at a speed, as from = move_towards(from, to, speed * dt).
    [[nodiscard]] inline auto move_towards(Vec2 from, Vec2 to, float max_step) noexcept -> Vec2 {
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

    [[nodiscard]] inline auto length(Vec3 v) noexcept -> float { return std::sqrt(dot(v, v)); }
    // A zero vector has no direction; normalizing one gives NaNs.
    [[nodiscard]] inline auto normalize(Vec3 v) noexcept -> Vec3 { return v / length(v); }

    [[nodiscard]] inline auto distance(Vec3 a, Vec3 b) noexcept -> float { return length(b - a); }

    // a at t = 0, b at t = 1, on along the line beyond them.
    [[nodiscard]] constexpr auto lerp(Vec3 a, Vec3 b, float t) noexcept -> Vec3 { return a + (b - a) * t; }

    // From towards to by max_step at most, landing on to rather than past it.
    [[nodiscard]] inline auto move_towards(Vec3 from, Vec3 to, float max_step) noexcept -> Vec3 {
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

    // Turns by angle radians around the axis, counter-clockwise when the axis
    // points at the viewer. The axis need not be unit length, only non-zero.
    [[nodiscard]] inline auto rotate(float angle, Vec3 axis) noexcept -> Mat4 {
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

    // Maps the box [left, right] x [bottom, top] x [-z_near, -z_far] onto clip
    // space, with no perspective: what 2D drawing wants. The same matrix as
    // glm::ortho, so z = 0 stays at 0 with the default depth range. The bounds
    // of each pair must differ. Not near/far: <windows.h> defines those as
    // macros.
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

    // A camera looking down -z with a vertical field of view of fov_y radians;
    // aspect is width over height. Both distances are positive, z_near the
    // smaller: depth precision is spent mostly near it, so keep it as large as
    // the scene allows. The same matrix as glm::perspective.
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

    // Moves the world so that a camera at eye looks at target, up being
    // roughly its up. up must not point along the view. The same matrix as
    // glm::lookAt.
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
