#include "core/geometry.h"

#include "support.h"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

using tgx_test::near;

TEST_CASE("circles get as many segments as keep them round") {
    using tgx::detail::segments_for;
    CHECK(segments_for(0.f) == tgx::detail::min_segments);
    CHECK(segments_for(1e30f) == tgx::detail::max_segments);
    CHECK(segments_for(10.f) <= segments_for(100.f));

    // Within a quarter pixel of the true circle.
    const float radius = 100.f;
    const auto n = static_cast<float>(segments_for(radius));
    CHECK(radius * (1.f - std::cos(std::numbers::pi_v<float> / n)) <= tgx::detail::circle_tolerance);
}

TEST_CASE("a canvas spans its own size, else its viewport, else the surface") {
    using tgx::detail::span_of;
    CHECK(span_of({320, 180}, {0, 0, 100, 100}, {1280, 720}) == tgx::Size{320, 180});
    CHECK(span_of({}, {10, 10, 199.6f, 150.2f}, {1280, 720}) == tgx::Size{200, 150});
    CHECK(span_of({}, {}, {1280, 720}) == tgx::Size{1280, 720});
}

TEST_CASE("line joints") {
    using tgx::detail::joint;
    const tgx::Vec2 at{10, 10};
    const float half = 2.f;

    SUBCASE("a right angle is mitered, its points the diagonal away") {
        const auto j = joint(at, {1, 0}, {0, 1}, half, 100.f);
        REQUIRE(j.mitered);
        CHECK(near(j.plus + j.minus, at * 2.f));
        CHECK(tgx::length(j.plus - at) == doctest::Approx(half * std::numbers::sqrt2_v<float>));
    }
    SUBCASE("straight on, the points are square across the line") {
        const auto j = joint(at, {1, 0}, {1, 0}, half, 100.f);
        REQUIRE(j.mitered);
        CHECK(tgx::length(j.plus - at) == doctest::Approx(half));
    }
    SUBCASE("turning straight back is not mitered") {
        CHECK_FALSE(joint(at, {1, 0}, {-1, 0}, half, 100.f).mitered);
    }
    SUBCASE("a corner sharper than the miter limit is not mitered") {
        const tgx::Vec2 back = tgx::normalize(tgx::Vec2{-1.f, 0.05f});
        CHECK_FALSE(joint(at, {1, 0}, back, half, 100.f).mitered);
    }
    SUBCASE("a line too short for its corner is not mitered") {
        const tgx::Vec2 sharp = tgx::normalize(tgx::Vec2{-1.f, 1.f});
        CHECK(joint(at, {1, 0}, sharp, half, 100.f).mitered);
        CHECK_FALSE(joint(at, {1, 0}, sharp, half, 1.f).mitered);
    }
}

TEST_CASE("quad colors blend flat when opposite corners add up alike") {
    using tgx::colors::black;
    using tgx::colors::green;
    using tgx::colors::red;
    CHECK(tgx::detail::blends_flat({red, red, black, black}));      // top to bottom
    CHECK(tgx::detail::blends_flat({red, black, black, red}));      // left to right
    CHECK_FALSE(tgx::detail::blends_flat({red, green, red, green}));
    CHECK(tgx::detail::average({red, green, red, green}) == tgx::Color{128, 128, 0, 255});
}

TEST_CASE("unit_direction") {
    using tgx::detail::unit_direction;
    CHECK(unit_direction(false, false, false, false) == tgx::Vec2{});
    CHECK(unit_direction(false, true, false, false) == tgx::Vec2{1, 0});
    CHECK(unit_direction(false, false, true, false) == tgx::Vec2{0, -1});
    CHECK(unit_direction(true, true, false, false) == tgx::Vec2{});
    CHECK(tgx::length(unit_direction(false, true, false, true)) == doctest::Approx(1.f));
}
