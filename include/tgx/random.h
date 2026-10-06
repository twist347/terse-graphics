#pragma once

#include "tgx/assert.h"
#include "tgx/math.h"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <ranges>

namespace tgx {
    // Random numbers for games: where things spawn, which way sparks fly,
    // whether a window lights up. A plain value: keep one per thing that
    // needs its own sequence, e.g. one for the level layout and one for
    // effects, so a replay of the layout does not depend on the effects.
    //
    // The same seed gives the same numbers on every platform and compiler:
    // the generator (PCG32) and the ways numbers are drawn from it are tgx's
    // own, unlike <random>'s distributions, which differ between standard
    // libraries. Not for anything secret.
    //
    //     tgx::Random random{42};
    //     const float x = random.next_float(0, 320);
    //     if (random.chance(0.1f)) { ... }
    class Random {
    public:
        // Different on every run, from the clock.
        Random() noexcept
            : Random{static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())} {
        }

        // The same numbers every time for the same seed.
        explicit Random(std::uint64_t seed) noexcept {
            step();
            m_state += seed;
            step();
        }

        // All 32 bits random.
        [[nodiscard]] auto next_u32() noexcept -> std::uint32_t {
            const std::uint64_t old = step();
            const auto shifted = static_cast<std::uint32_t>(((old >> 18) ^ old) >> 27);
            const auto rotation = static_cast<std::uint32_t>(old >> 59);
            return std::rotr(shifted, static_cast<int>(rotation));
        }

        // In [lo, hi): lo may come up, hi never does, unless they are equal.
        [[nodiscard]] auto next_float(float lo, float hi) noexcept -> float {
            TGX_ASSERT_MSG(lo <= hi, "next_float({}, {}): an empty range", lo, hi);

            // The top 24 bits, as many as a float holds exactly.
            const float unit = static_cast<float>(next_u32() >> 8) * 0x1p-24f;
            const float value = lo + (hi - lo) * unit;
            // Rounding can reach hi itself when the range is wide.
            return value < hi ? value : std::nextafter(hi, lo);
        }

        // In [lo, hi], both ends included, as a die is: next_int(1, 6).
        [[nodiscard]] auto next_int(int lo, int hi) noexcept -> int {
            TGX_ASSERT_MSG(lo <= hi, "next_int({}, {}): an empty range", lo, hi);

            // Lemire's way, without the bias a plain modulo has.
            const auto range = static_cast<std::uint32_t>(static_cast<std::int64_t>(hi) - lo + 1);
            if (range == 0) {
                // The whole of int: every 32 bits are fine.
                return static_cast<int>(next_u32());
            }
            std::uint64_t product = static_cast<std::uint64_t>(next_u32()) * range;
            auto low = static_cast<std::uint32_t>(product);
            if (low < range) {
                const std::uint32_t threshold = (0u - range) % range;
                while (low < threshold) {
                    product = static_cast<std::uint64_t>(next_u32()) * range;
                    low = static_cast<std::uint32_t>(product);
                }
            }
            return static_cast<int>(static_cast<std::int64_t>(lo) + static_cast<std::int64_t>(product >> 32));
        }

        // True with the probability p: chance(0.25f) about once in four.
        [[nodiscard]] auto chance(float p) noexcept -> bool {
            return next_float(0.f, 1.f) < p;
        }

        // A point anywhere inside the rectangle.
        [[nodiscard]] auto point_in(Rect rect) noexcept -> Vec2 {
            return {next_float(rect.x, rect.right()), next_float(rect.y, rect.bottom())};
        }

        // A unit vector pointing any way, all ways alike.
        [[nodiscard]] auto direction() noexcept -> Vec2 {
            return from_angle(next_float(0.f, 2.f * std::numbers::pi_v<float>));
        }

        // One of the items of an array, a vector, a span, each as likely;
        // there must be at least one. Given by reference, so not from a
        // temporary container, which is gone by the time it is used.
        //
        //     const tgx::Color color = random.pick(palette);
        template<std::ranges::random_access_range Items>
            requires std::ranges::sized_range<Items> && std::ranges::borrowed_range<Items>
        [[nodiscard]] auto pick(Items &&items) noexcept -> std::ranges::range_reference_t<Items> {
            TGX_ASSERT_MSG(!std::ranges::empty(items), "picking from no items");

            const int last = static_cast<int>(std::ranges::size(items)) - 1;
            return std::ranges::begin(items)[next_int(0, last)];
        }

    private:
        // Which of PCG's streams; any odd number, fixed so a seed alone
        // decides the numbers.
        static constexpr std::uint64_t increment = 1442695040888963407ull;

        // Moves the state on, giving the one it had.
        auto step() noexcept -> std::uint64_t {
            const std::uint64_t old = m_state;
            m_state = old * 6364136223846793005ull + increment;
            return old;
        }

        std::uint64_t m_state{0};
    };
}
