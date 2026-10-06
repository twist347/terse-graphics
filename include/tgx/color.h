#pragma once

#include "tgx/assert.h"
#include "tgx/math.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tgx {
    struct Color {
        std::uint8_t r{0};
        std::uint8_t g{0};
        std::uint8_t b{0};
        std::uint8_t a{255};

        // 0xRRGGBBAA, the order colors are written in editors and palettes.
        [[nodiscard]] static constexpr auto rgba(std::uint32_t rgba) noexcept -> Color {
            return {
                static_cast<std::uint8_t>((rgba >> 24u) & 0xFFu),
                static_cast<std::uint8_t>((rgba >> 16u) & 0xFFu),
                static_cast<std::uint8_t>((rgba >> 8u) & 0xFFu),
                static_cast<std::uint8_t>(rgba & 0xFFu),
            };
        }

        // 0xRRGGBB, fully opaque.
        [[nodiscard]] static constexpr auto rgb(std::uint32_t rgb) noexcept -> Color {
            return rgba((rgb << 8u) | 0xFFu);
        }

        // Hue in degrees around the color wheel (0 red, 120 green, 240 blue,
        // any value wraps), saturation and value in [0, 1]; fully opaque.
        // Stepping the hue gives evenly spread, equally bright colors.
        [[nodiscard]] static auto hsv(float hue, float saturation, float value) noexcept -> Color {
            // A NaN would pass the clamps below and turn into no channel value.
            TGX_ASSERT_MSG(
                std::isfinite(hue) && std::isfinite(saturation) && std::isfinite(value),
                "hsv({}, {}, {}): not a number",
                hue, saturation, value
            );

            float h = std::fmod(hue, 360.f);
            if (h < 0.f) {
                h += 360.f;
            }
            const float s = std::clamp(saturation, 0.f, 1.f);
            const float v = std::clamp(value, 0.f, 1.f);
            const auto channel = [&](float n) {
                const float k = std::fmod(n + h / 60.f, 6.f);
                const float f = v - v * s * std::clamp(std::min(k, 4.f - k), 0.f, 1.f);
                return static_cast<std::uint8_t>(f * 255.f + 0.5f);
            };
            return {channel(5.f), channel(3.f), channel(1.f), 255};
        }

        // The same color with another alpha, e.g. colors::red.with_alpha(128).
        [[nodiscard]] constexpr auto with_alpha(std::uint8_t alpha) const noexcept -> Color {
            return {r, g, b, alpha};
        }

        // The same color with alpha from [0, 1] (clamped), replacing its own as
        // raylib's Fade does: colors::red.fade(0.5f) is half see-through.
        [[nodiscard]] constexpr auto fade(float alpha) const noexcept -> Color {
            // A NaN, as from t / duration with no duration, would pass the clamp.
            TGX_ASSERT_MSG(std::isfinite(alpha), "fade({}): not a number", alpha);
            return with_alpha(static_cast<std::uint8_t>(std::clamp(alpha, 0.f, 1.f) * 255.f + 0.5f));
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

    // a at t = 0, b at t = 1, every channel and alpha alike. Unlike the vector
    // lerps, t is clamped to [0, 1]: past the ends a channel has nowhere to go.
    [[nodiscard]] constexpr auto lerp(Color a, Color b, float t) noexcept -> Color {
        // A NaN would pass the clamp and turn into no channel value.
        TGX_ASSERT_MSG(std::isfinite(t), "lerp(a, b, {}): not a number", t);
        const float k = std::clamp(t, 0.f, 1.f);
        const auto mix = [k](std::uint8_t from, std::uint8_t to) {
            const auto f = static_cast<float>(from);
            return static_cast<std::uint8_t>(f + (static_cast<float>(to) - f) * k + 0.5f);
        };
        return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
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
        inline constexpr Color orange{255, 165, 0, 255};
        inline constexpr Color brown{165, 42, 42, 255};
        inline constexpr Color cyan{0, 255, 255, 255};
        inline constexpr Color magenta{255, 0, 255, 255};
        inline constexpr Color purple{128, 0, 128, 255};
        inline constexpr Color pink{255, 192, 203, 255};
        inline constexpr Color gray{128, 128, 128, 255};
        inline constexpr Color light_gray{192, 192, 192, 255};
        inline constexpr Color dark_gray{64, 64, 64, 255};
    }
}
