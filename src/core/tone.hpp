#pragma once

#include "tgx/core/assert.hpp"
#include "tgx/core/audio.hpp"
#include "tgx/core/random.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

// The samples of Sound::tone, apart from the audio output so tests can use them.
namespace tgx::detail {
    inline constexpr int tone_rate = 44100;
    // Always faded out over the last of it, so it ends without a click.
    inline constexpr double tone_end_fade = 0.005;
    // Any fixed seed: the same noise on every call.
    inline constexpr unsigned tone_noise_seed = 1;

    // One period's shape at phase in [0, 1), in [-1, 1]. All but square and
    // noise start at 0 and rise.
    [[nodiscard]] inline auto wave_at(Waveform waveform, double phase) noexcept -> double {
        switch (waveform) {
        case Waveform::sine:
            return std::sin(2 * std::numbers::pi * phase);
        case Waveform::square:
            return phase < 0.5 ? 1 : -1;
        case Waveform::triangle: {
            const double shifted = phase + 0.25;
            return 4 * std::abs(shifted - std::floor(shifted + 0.5)) - 1;
        }
        case Waveform::sawtooth:
            return 2 * (phase - std::floor(phase + 0.5));
        case Waveform::noise:
            break;
        }
        return 0;
    }

    [[nodiscard]] inline auto tone_samples(const ToneParams &params) -> std::vector<float> {
        const ToneParams &p = params;
        TGX_ASSERT_MSG(
            std::isfinite(p.frequency) && p.frequency > 0 && p.frequency <= tone_rate / 2,
            "tone of {} Hz: not in (0, {}]",
            p.frequency, tone_rate / 2
        );
        TGX_ASSERT_MSG(std::isfinite(p.duration) && p.duration > 0, "tone of {} s", p.duration);
        TGX_ASSERT_MSG(p.volume >= 0 && p.volume <= 1, "tone volume {}: not in [0, 1]", p.volume);
        TGX_ASSERT_MSG(std::isfinite(p.attack) && p.attack >= 0, "tone attack {} s", p.attack);
        TGX_ASSERT_MSG(std::isfinite(p.decay) && p.decay >= 0, "tone decay {}", p.decay);

        const auto count = static_cast<std::size_t>(std::max(std::lround(p.duration * tone_rate), 1L));
        const double end_fade = std::min(tone_end_fade, p.duration / 2.0);
        std::vector<float> samples(count);

        Random random{tone_noise_seed};
        double noise = 0;
        long noise_step = -1;

        for (std::size_t i = 0; i < count; ++i) {
            const double t = static_cast<double>(i) / tone_rate;
            const double cycles = t * p.frequency;
            const double phase = cycles - std::floor(cycles);

            double wave = 0;
            if (p.waveform == Waveform::noise) {
                // A new level every half period, as a square changes.
                const auto step = static_cast<long>(cycles * 2);
                if (step != noise_step) {
                    noise_step = step;
                    noise = random.next_float(-1.f, 1.f);
                }
                wave = noise;
            } else {
                wave = wave_at(p.waveform, phase);
            }

            const double to_end = static_cast<double>(count - 1 - i) / tone_rate;
            const double rise = p.attack > 0 ? std::min(t / p.attack, 1.0) : 1.0;
            const double fall = end_fade > 0 ? std::min(to_end / end_fade, 1.0) : 1.0;
            const double level = p.volume * rise * fall * std::exp(-p.decay * t);
            samples[i] = static_cast<float>(level * wave);
        }
        return samples;
    }
}
