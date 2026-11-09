#pragma once

#include "tgx/core/assert.hpp"
#include "tgx/core/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace tgx {
    struct Color {
        std::uint8_t r{0};
        std::uint8_t g{0};
        std::uint8_t b{0};
        std::uint8_t a{255};

        // 0xRRGGBBAA, as editors write colors.
        [[nodiscard]] static constexpr auto rgba(std::uint32_t rgba) noexcept -> Color {
            return {
                static_cast<std::uint8_t>((rgba >> 24) & 0xff),
                static_cast<std::uint8_t>((rgba >> 16) & 0xff),
                static_cast<std::uint8_t>((rgba >> 8) & 0xff),
                static_cast<std::uint8_t>(rgba & 0xff),
            };
        }

        // 0xRRGGBB, fully opaque.
        [[nodiscard]] static constexpr auto rgb(std::uint32_t rgb) noexcept -> Color {
            return rgba((rgb << 8) | 0xff);
        }

        // Hue in radians (0 red, 2pi / 3 green, 4pi / 3 blue; wraps around).
        // Saturation and value in [0, 1]. Opaque. Stepping the hue gives
        // evenly spread, equally bright colors.
        [[nodiscard]] static auto hsv(float hue, float saturation, float value) noexcept -> Color {
            // A NaN would pass the clamps.
            TGX_ASSERT_MSG(
                std::isfinite(hue) && std::isfinite(saturation) && std::isfinite(value),
                "hsv({}, {}, {}): not a number",
                hue, saturation, value
            );

            constexpr float full_turn = 2.f * std::numbers::pi_v<float>;
            float h = std::fmod(hue, full_turn);
            if (h < 0.f) {
                h += full_turn;
            }
            const float s = std::clamp(saturation, 0.f, 1.f);
            const float v = std::clamp(value, 0.f, 1.f);
            const auto channel = [&](float n) {
                const float k = std::fmod(n + h / (full_turn / 6.f), 6.f);
                const float f = v - v * s * std::clamp(std::min(k, 4.f - k), 0.f, 1.f);
                return static_cast<std::uint8_t>(f * 255.f + 0.5f);
            };
            return {channel(5.f), channel(3.f), channel(1.f), 255};
        }

        // E.g. colors::red.with_alpha(128).
        [[nodiscard]] constexpr auto with_alpha(std::uint8_t alpha) const noexcept -> Color {
            return {r, g, b, alpha};
        }

        // Replaces alpha with one in [0, 1] (clamped), like raylib's Fade:
        // colors::red.fade(0.5f) is half transparent.
        [[nodiscard]] auto fade(float alpha) const noexcept -> Color {
            // A NaN, e.g. t / duration with zero duration, would pass the
            // clamp.
            TGX_ASSERT_MSG(std::isfinite(alpha), "fade({}): not a number", alpha);
            return with_alpha(static_cast<std::uint8_t>(std::clamp(alpha, 0.f, 1.f) * 255.f + 0.5f));
        }

        // Channels multiplied by alpha, as Blend::premultiplied expects.
        [[nodiscard]] constexpr auto premultiplied() const noexcept -> Color {
            const auto scale = [this](std::uint8_t channel) {
                return static_cast<std::uint8_t>((channel * a + 127) / 255);
            };
            return {scale(r), scale(g), scale(b), a};
        }

        [[nodiscard]] constexpr auto operator==(const Color &) const noexcept -> bool = default;
    };

    // Channels in [0, 1], as shaders see them.
    [[nodiscard]] constexpr auto to_vec4(Color color) noexcept -> Vec4 {
        return {
            static_cast<float>(color.r) / 255.f,
            static_cast<float>(color.g) / 255.f,
            static_cast<float>(color.b) / 255.f,
            static_cast<float>(color.a) / 255.f,
        };
    }

    // Every channel, alpha too. Like the vector lerps, t is not clamped;
    // channels that extrapolate past a byte's range are clamped.
    [[nodiscard]] inline auto lerp(Color a, Color b, float t) noexcept -> Color {
        TGX_ASSERT_MSG(std::isfinite(t), "lerp(a, b, {}): not a number", t);
        const auto mix = [t](std::uint8_t from, std::uint8_t to) {
            const auto f = static_cast<float>(from);
            const float value = f + (static_cast<float>(to) - f) * t;
            return static_cast<std::uint8_t>(std::clamp(value, 0.f, 255.f) + 0.5f);
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
        inline constexpr Color maroon{128, 0, 0, 255};
        inline constexpr Color dark_green{0, 128, 0, 255};
        inline constexpr Color navy{0, 0, 128, 255};
        inline constexpr Color olive{128, 128, 0, 255};
        inline constexpr Color teal{0, 128, 128, 255};
        inline constexpr Color pink{255, 192, 203, 255};
        inline constexpr Color gray{128, 128, 128, 255};
        inline constexpr Color light_gray{192, 192, 192, 255};
        inline constexpr Color dark_gray{64, 64, 64, 255};
    }
}
