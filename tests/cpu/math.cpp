#include "tgx/core/math.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <array>
#include <numbers>
#include <string_view>

using tgx_test::near;

namespace {
    constexpr float pi = std::numbers::pi_v<float>;
}

TEST_CASE("Vec2 arithmetic") {
    const tgx::Vec2 a{1, 2};
    const tgx::Vec2 b{3, -1};
    CHECK(a + b == tgx::Vec2{4, 1});
    CHECK(a - b == tgx::Vec2{-2, 3});
    CHECK(-a == tgx::Vec2{-1, -2});
    CHECK(a * 2.f == tgx::Vec2{2, 4});
    CHECK(2.f * a == tgx::Vec2{2, 4});
    CHECK(a / 2.f == tgx::Vec2{0.5f, 1});

    tgx::Vec2 c = a;
    c += b;
    CHECK(c == tgx::Vec2{4, 1});
}

TEST_CASE("Vec2 products and lengths") {
    CHECK(tgx::dot(tgx::Vec2{1, 2}, tgx::Vec2{3, 4}) == 11.f);
    // Positive when b turns clockwise from a on y-down screen coordinates.
    CHECK(tgx::cross(tgx::Vec2{1, 0}, tgx::Vec2{0, 1}) > 0.f);
    CHECK(tgx::cross(tgx::Vec2{0, 1}, tgx::Vec2{1, 0}) < 0.f);
    CHECK(tgx::cross(tgx::Vec2{2, 2}, tgx::Vec2{1, 1}) == 0.f);
    CHECK(tgx::length(tgx::Vec2{3, 4}) == 5.f);
    CHECK(tgx::distance(tgx::Vec2{1, 1}, tgx::Vec2{4, 5}) == 5.f);
    CHECK(near(tgx::normalize(tgx::Vec2{0, 7}), {0, 1}));
}

TEST_CASE("Vec2 angles") {
    CHECK(tgx::from_angle(0.f) == tgx::Vec2{1, 0});
    CHECK(near(tgx::from_angle(pi / 2), {0, 1}));
    CHECK(tgx::angle(tgx::Vec2{0, 1}) == doctest::Approx(pi / 2));
    // A zero vector has no direction, whatever the sign of its zeros.
    CHECK(tgx::angle(tgx::Vec2{}) == 0.f);
    CHECK(tgx::angle(tgx::Vec2{-0.f, 0.f}) == 0.f);
    CHECK(near(tgx::rotate(tgx::Vec2{1, 0}, pi / 2), {0, 1}));
    CHECK(tgx::perpendicular(tgx::Vec2{1, 0}) == tgx::Vec2{0, -1});
}

TEST_CASE("lerp and move_towards") {
    CHECK(tgx::lerp(tgx::Vec2{0, 0}, tgx::Vec2{10, 20}, 0.5f) == tgx::Vec2{5, 10});
    // Vector lerps go on past the ends.
    CHECK(tgx::lerp(tgx::Vec2{0, 0}, tgx::Vec2{10, 0}, 2.f) == tgx::Vec2{20, 0});

    CHECK(tgx::move_towards(tgx::Vec2{0, 0}, tgx::Vec2{10, 0}, 3.f) == tgx::Vec2{3, 0});
    // Lands on the target rather than past it.
    CHECK(tgx::move_towards(tgx::Vec2{0, 0}, tgx::Vec2{10, 0}, 30.f) == tgx::Vec2{10, 0});
    CHECK(tgx::move_towards(tgx::Vec2{5, 5}, tgx::Vec2{5, 5}, 1.f) == tgx::Vec2{5, 5});
}

TEST_CASE("Vec3") {
    CHECK(tgx::cross(tgx::Vec3{1, 0, 0}, tgx::Vec3{0, 1, 0}) == tgx::Vec3{0, 0, 1});
    CHECK(tgx::dot(tgx::Vec3{1, 2, 3}, tgx::Vec3{4, 5, 6}) == 32.f);
    CHECK(tgx::length(tgx::Vec3{2, 3, 6}) == 7.f);
}

TEST_CASE("Size and Rect") {
    CHECK(tgx::Size{}.empty());
    CHECK(tgx::Size{0, 10}.empty());
    CHECK_FALSE(tgx::Size{1, 1}.empty());
    CHECK(tgx::Size{1280, 720}.aspect() == doctest::Approx(16.f / 9.f));
    // A minimized window must not put inf into a projection.
    CHECK(tgx::Size{0, 0}.aspect() == 1.f);

    const tgx::Rect r{10, 20, 30, 40};
    CHECK(r.right() == 40.f);
    CHECK(r.bottom() == 60.f);
    CHECK(r.center() == tgx::Vec2{25, 40});
    static_assert(tgx::Rect::from_center({25, 40}, {30, 40}) == tgx::Rect{10, 20, 30, 40});
    CHECK(tgx::Rect::from_center(r.center(), {30, 40}) == r);
}

TEST_CASE("Mat4") {
    const tgx::Vec4 p{1, 2, 3, 1};
    CHECK(tgx::Mat4{} * p == p);
    CHECK(tgx::translate({10, 0, 0}) * p == tgx::Vec4{11, 2, 3, 1});
    // b first, then a: the point is scaled, then moved.
    CHECK(tgx::translate({10, 0, 0}) * tgx::scale({2, 2, 2}) * p == tgx::Vec4{12, 4, 6, 1});
    CHECK(near(tgx::rotate({0, 0, 1}, pi / 2) * tgx::Vec4{1, 0, 0, 1}, {0, 1, 0, 1}));
}

TEST_CASE("ortho as 2D drawing uses it") {
    // The canvas's projection: (0, 0) at the top-left, y down.
    const tgx::Mat4 m = tgx::ortho(0, 320, 180, 0);
    CHECK(near(m * tgx::Vec4{0, 0, 0, 1}, {-1, 1, 0, 1}));
    CHECK(near(m * tgx::Vec4{320, 180, 0, 1}, {1, -1, 0, 1}));
    CHECK(near(m * tgx::Vec4{160, 90, 0, 1}, {0, 0, 0, 1}));
}

TEST_CASE("fit keeps the proportions, in the middle") {
    // 16:9 into a square: full width, bars above and below.
    CHECK(tgx::fit({320, 180}, {640, 640}) == tgx::Rect{0, 140, 640, 360});
    // Taller area than wide content needs: bars at the sides.
    CHECK(tgx::fit({100, 100}, {300, 200}) == tgx::Rect{50, 0, 200, 200});
    CHECK(tgx::fit({}, {100, 100}) == tgx::Rect{});
}

TEST_CASE("fit_whole scales by a whole factor") {
    CHECK(tgx::fit_whole({320, 180}, {1366, 768}) == tgx::Rect{43, 24, 1280, 720});
    CHECK(tgx::fit_whole({320, 180}, {1280, 720}) == tgx::Rect{0, 0, 1280, 720});
    // Smaller than the content: still once, overflowing.
    CHECK(tgx::fit_whole({320, 180}, {200, 100}).width == 320.f);
    CHECK(tgx::fit_whole({320, 180}, {0, 0}) == tgx::Rect{});
}

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
