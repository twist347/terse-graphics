#pragma once

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

        [[nodiscard]] constexpr auto operator==(const Color &) const noexcept -> bool = default;
    };

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
