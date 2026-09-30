#pragma once

#include <array>
#include <type_traits>

namespace tgx {
    // Plain float vectors and a matrix, laid out as GLSL and glm lay them out:
    // they go to the GPU byte for byte, and glm values convert with std::bit_cast.
    // No arithmetic yet; it comes as the library needs it.

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

    // Column-major, like GLSL's mat4: columns[c] is column c, so a translation
    // sits in columns[3]. Starts as the identity, so a matrix left unset draws
    // untransformed rather than collapsing everything to a point.
    struct Mat4 {
        std::array<Vec4, 4> columns{{
            {1.f, 0.f, 0.f, 0.f},
            {0.f, 1.f, 0.f, 0.f},
            {0.f, 0.f, 1.f, 0.f},
            {0.f, 0.f, 0.f, 1.f},
        }};

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
}
