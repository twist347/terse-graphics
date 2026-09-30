#pragma once

#include "tgx/math.h"

#include <cstdint>

namespace tgx {
    struct Color {
        std::uint8_t r{0};
        std::uint8_t g{0};
        std::uint8_t b{0};
        std::uint8_t a{255};

        // 0xRRGGBBAA, the order colors are written in editors and palettes.
        [[nodiscard]] static constexpr auto hex(std::uint32_t rgba) noexcept -> Color {
            return {
                static_cast<std::uint8_t>((rgba >> 24U) & 0xFFU),
                static_cast<std::uint8_t>((rgba >> 16U) & 0xFFU),
                static_cast<std::uint8_t>((rgba >> 8U) & 0xFFU),
                static_cast<std::uint8_t>(rgba & 0xFFU),
            };
        }

        // 0xRRGGBB, fully opaque.
        [[nodiscard]] static constexpr auto rgb(std::uint32_t rgb) noexcept -> Color {
            return hex((rgb << 8U) | 0xFFU);
        }

        // The same color with another alpha, e.g. colors::red.with_alpha(128).
        [[nodiscard]] constexpr auto with_alpha(std::uint8_t alpha) const noexcept -> Color {
            return {r, g, b, alpha};
        }

        // Each channel scaled by alpha, rounded: the form Blend::premultiplied
        // expects. Alpha itself stays.
        [[nodiscard]] constexpr auto premultiplied() const noexcept -> Color {
            const auto scale = [this](std::uint8_t channel) {
                return static_cast<std::uint8_t>((channel * a + 127) / 255);
            };
            return {scale(r), scale(g), scale(b), a};
        }

        [[nodiscard]] constexpr auto operator==(const Color &) const noexcept -> bool = default;
    };

    // Channels scaled to [0, 1], the way shaders see a Color.
    [[nodiscard]] constexpr auto to_vec4(Color color) noexcept -> Vec4 {
        return {
            static_cast<float>(color.r) / 255.f,
            static_cast<float>(color.g) / 255.f,
            static_cast<float>(color.b) / 255.f,
            static_cast<float>(color.a) / 255.f,
        };
    }

    static_assert(sizeof(Color) == 4);
    static_assert(alignof(Color) == 1);

    namespace colors {
        inline constexpr Color transparent{0, 0, 0, 0};
        inline constexpr Color black{0, 0, 0, 255};
        inline constexpr Color white{255, 255, 255, 255};
        inline constexpr Color red{255, 0, 0, 255};
        inline constexpr Color green{0, 255, 0, 255};
        inline constexpr Color blue{0, 0, 255, 255};
        inline constexpr Color yellow{255, 255, 0, 255};
        inline constexpr Color cyan{0, 255, 255, 255};
        inline constexpr Color magenta{255, 0, 255, 255};
        inline constexpr Color gray{128, 128, 128, 255};
        inline constexpr Color light_gray{192, 192, 192, 255};
        inline constexpr Color dark_gray{64, 64, 64, 255};
    }
}
