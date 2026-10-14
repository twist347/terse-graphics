// Easing curves: their ends, clamping, symmetry and shape.

#include "tgx/core/ease.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string_view>

TEST_CASE("easing") {
    using Curve = float (*)(float) noexcept;
    struct Family {
        const char *name;
        Curve in;
        Curve out;
        Curve in_out;
        bool overshoots;
    };
    namespace ease = tgx::ease;
    const std::array families{
        Family{"sine", ease::in_sine, ease::out_sine, ease::in_out_sine, false},
        Family{"quad", ease::in_quad, ease::out_quad, ease::in_out_quad, false},
        Family{"cubic", ease::in_cubic, ease::out_cubic, ease::in_out_cubic, false},
        Family{"expo", ease::in_expo, ease::out_expo, ease::in_out_expo, false},
        Family{"back", ease::in_back, ease::out_back, ease::in_out_back, true},
        Family{"elastic", ease::in_elastic, ease::out_elastic, ease::in_out_elastic, true},
        Family{"bounce", ease::in_bounce, ease::out_bounce, ease::in_out_bounce, false},
    };

    for (const Family &family: families) {
        INFO(family.name);
        for (const Curve curve: {family.in, family.out, family.in_out}) {
            // The ends, and t clamped beyond them.
            CHECK(curve(0.f) == doctest::Approx(0.f).epsilon(1e-6));
            CHECK(curve(1.f) == doctest::Approx(1.f).epsilon(1e-6));
            CHECK(curve(-1.f) == doctest::Approx(0.f).epsilon(1e-6));
            CHECK(curve(2.f) == doctest::Approx(1.f).epsilon(1e-6));
        }
        CHECK(family.in_out(0.5f) == doctest::Approx(0.5f));

        bool steady = true;
        bool beyond = false;
        float previous_in = 0.f;
        float previous_out = 0.f;
        for (int i = 1; i <= 100; ++i) {
            const float t = static_cast<float>(i) / 100.f;
            // out mirrors in.
            CHECK(family.out(t) == doctest::Approx(1.f - family.in(1.f - t)).epsilon(1e-5));
            // Bounce dips back between landings but stays in [0, 1].
            steady = steady && family.in(t) >= previous_in - 1e-6f && family.out(t) >= previous_out - 1e-6f;
            beyond = beyond || family.in(t) < 0.f || family.out(t) > 1.f;
            previous_in = family.in(t);
            previous_out = family.out(t);
        }
        if (std::string_view{family.name} != "bounce") {
            CHECK(steady == !family.overshoots);
        }
        CHECK(beyond == family.overshoots);
    }

    CHECK(ease::linear(0.25f) == 0.25f);
    CHECK(ease::linear(3.f) == 1.f);
    CHECK(ease::out_cubic(0.5f) > 0.5f);
    CHECK(ease::in_cubic(0.5f) < 0.5f);
    static_assert(ease::in_quad(0.f) == 0.f && ease::in_quad(1.f) == 1.f);
    static_assert(ease::out_back(0.f) == 0.f && ease::out_bounce(1.f) > 0.999f);
}
