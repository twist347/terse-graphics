#pragma once

#include "tgx/core/assert.hpp"

#include <cmath>
#include <numbers>

// Curves that bend progress t (0 to 1) so motion starts or ends softly,
// for lerp() or std::lerp:
//
//     pos = tgx::lerp(from, to, tgx::ease::out_cubic(timer.progress()));
//
// in: slow start, out: slow end, in_out: both. t is clamped to [0, 1].
// Every curve gives 0 at 0 and 1 at 1; back and elastic overshoot in
// between, by design.
namespace tgx::ease {
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
