#pragma once

#include "tgx/core/assert.hpp"
#include "tgx/core/math.hpp"

#include <bit>
#include <chrono>
#include <concepts>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <ranges>

namespace tgx {
    // Random numbers for games, as a plain value. Keep one per sequence, e.g.
    // one for the level layout and one for effects, so the layout replays the
    // same whatever the effects do.
    //
    // The same seed gives the same numbers on every platform and compiler:
    // the generator (PCG32) and the distributions are tgx's own, unlike
    // <random>'s. Not for anything secret.
    //
    //     tgx::Random random{42};
    //     const float x = random.next_float(0, 320);
    //     if (random.chance(0.1f)) { ... }
    class Random {
    public:
        // Seeded from the clock: different on every run.
        Random() noexcept
            : Random{static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())} {
        }

        // Any integer type, so Random{level} is not a narrowing error. A
        // negative seed is used as its bits.
        template<std::integral T>
        explicit Random(T seed) noexcept {
            step();
            m_state += static_cast<std::uint64_t>(seed);
            step();
        }

        // All 32 bits are random.
        [[nodiscard]] auto next_u32() noexcept -> std::uint32_t {
            const std::uint64_t old = step();
            const auto shifted = static_cast<std::uint32_t>(((old >> 18) ^ old) >> 27);
            const auto rotation = static_cast<std::uint32_t>(old >> 59);
            return std::rotr(shifted, static_cast<int>(rotation));
        }

        // In [lo, hi): hi never comes up, unless lo == hi.
        [[nodiscard]] auto next_float(float lo, float hi) noexcept -> float {
            TGX_ASSERT_MSG(lo <= hi, "next_float({}, {}): an empty range", lo, hi);

            // 24 bits: as many as a float holds exactly.
            const float unit = static_cast<float>(next_u32() >> 8) * 0x1p-24f;
            const float value = lo + (hi - lo) * unit;
            // Rounding can give hi itself when the range is wide.
            return value < hi ? value : std::nextafter(hi, lo);
        }

        // In [lo, hi], like a die: next_int(1, 6).
        [[nodiscard]] auto next_int(int lo, int hi) noexcept -> int {
            TGX_ASSERT_MSG(lo <= hi, "next_int({}, {}): an empty range", lo, hi);

            // Lemire's method: no modulo bias.
            const auto range = static_cast<std::uint32_t>(static_cast<std::int64_t>(hi) - lo + 1);
            if (range == 0) {
                // The whole int range: any 32 bits will do.
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

        // chance(0.25f) is true about once in four.
        [[nodiscard]] auto chance(float p) noexcept -> bool {
            return next_float(0.f, 1.f) < p;
        }

        // Anywhere inside the rectangle.
        [[nodiscard]] auto point_in(Rect rect) noexcept -> Vec2 {
            return {next_float(rect.x, rect.right()), next_float(rect.y, rect.bottom())};
        }

        // A unit vector, all directions equally likely.
        [[nodiscard]] auto direction() noexcept -> Vec2 {
            return from_angle(next_float(0.f, 2.f * std::numbers::pi_v<float>));
        }

        // One item of an array, vector or span, each equally likely; at least
        // one (asserted). Returned by reference, so not from a temporary
        // container.
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
        // PCG's stream. Any odd number works; fixed so the seed alone decides.
        static constexpr std::uint64_t increment = 1442695040888963407ull;

        // Returns the previous state.
        auto step() noexcept -> std::uint64_t {
            const std::uint64_t old = m_state;
            m_state = old * 6364136223846793005ull + increment;
            return old;
        }

        std::uint64_t m_state{0};
    };
}
