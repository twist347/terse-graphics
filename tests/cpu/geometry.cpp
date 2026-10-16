#include "core/geometry.hpp"

#include "support.hpp"

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

TEST_CASE("arcs") {
    using tgx::detail::arc_between;
    using tgx::detail::whole_turn;

    SUBCASE("ends in either order, at most a full turn") {
        CHECK(arc_between(2.f, 1.f).start == 1.f);
        CHECK(arc_between(2.f, 1.f).sweep == 1.f);
        CHECK(arc_between(0.f, 3.f * whole_turn).sweep == whole_turn);
        CHECK(tgx::detail::full_turn(arc_between(0.f, 3.f * whole_turn)));
    }
    SUBCASE("a part of the circle's segments, at least one") {
        CHECK(tgx::detail::arc_segments({0.f, whole_turn / 4.f}, 64) == 16);
        CHECK(tgx::detail::arc_segments({0.f, 0.f}, 64) == 1);
    }
    SUBCASE("a rim runs clockwise from the start") {
        const tgx::detail::Rim rim{{10, 10}, 2.f, tgx::detail::full_circle, 4};
        CHECK(near(rim(0), {12, 10}));
        CHECK(near(rim(1), {10, 12}));
        CHECK(near(rim(4), rim(0)));
    }
}

TEST_CASE("ellipse rims") {
    using tgx::detail::EllipseRim;
    const tgx::Vec2 center{100, 50};

    SUBCASE("points lie on the radii, turned") {
        const EllipseRim rim{center, {40, 10}, {1, 0}, 4};
        CHECK(near(rim(0), {140, 50}));
        CHECK(near(rim(1), {100, 60}));
        const EllipseRim turned{center, {40, 10}, tgx::from_angle(std::numbers::pi_v<float> / 2.f), 4};
        CHECK(near(turned(0), {100, 90}));
    }
    SUBCASE("inset moves the axis points straight in") {
        const EllipseRim rim{center, {40, 10}, {1, 0}, 4, 3.f};
        CHECK(near(rim(0), {137, 50}));
        CHECK(near(rim(1), {100, 57}));
    }
    SUBCASE("inset past the end curvature keeps each point on its own side") {
        const std::size_t n = 256;
        const EllipseRim edge{center, {40, 4}, {1, 0}, n};
        const EllipseRim inset{center, {40, 4}, {1, 0}, n, 3.f};
        for (std::size_t i = 0; i < n; ++i) {
            const tgx::Vec2 e = edge(i) - center;
            const tgx::Vec2 p = inset(i) - center;
            CHECK(e.x * p.x >= 0.f);
            CHECK(e.y * p.y >= 0.f);
            CHECK(std::abs(p.x) <= 37.f + 1e-4f);
            CHECK(std::abs(p.y) <= 1.f + 1e-4f);
        }
    }
}

TEST_CASE("corners") {
    SUBCASE("clockwise from the top-left") {
        const auto box = tgx::detail::corners(tgx::Rect{1, 2, 3, 4});
        CHECK(box[0] == tgx::Vec2{1, 2});
        CHECK(box[1] == tgx::Vec2{4, 2});
        CHECK(box[2] == tgx::Vec2{4, 6});
        CHECK(box[3] == tgx::Vec2{1, 6});
    }
    SUBCASE("placed: origin at position, turned around it") {
        const auto box = tgx::detail::placed_corners({4, 2}, {2, 1}, {10, 10}, std::numbers::pi_v<float> / 2.f);
        CHECK(near(box[0], {11, 8}));
        CHECK(near(box[2], {9, 12}));
    }
    SUBCASE("a rounded rim starts at the left end of the top-left corner") {
        const tgx::Rect rect{0, 0, 100, 50};
        const std::size_t points = tgx::detail::corner_points(32);
        CHECK(points == 9);
        CHECK(near(tgx::detail::rounded_rim(rect, 10.f, 0.f, points, 0), {0, 10}));
        CHECK(near(tgx::detail::rounded_rim(rect, 10.f, 0.f, points, points - 1), {10, 0}));
        CHECK(near(tgx::detail::rounded_rim(rect, 10.f, 4.f, points, 0), {4, 10}));
        // Inset past the radius: a square corner.
        CHECK(near(tgx::detail::rounded_rim(rect, 10.f, 12.f, points, 0), {12, 12}));
    }
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
        REQUIRE(j);
        CHECK(near(j->plus + j->minus, at * 2.f));
        CHECK(tgx::length(j->plus - at) == doctest::Approx(half * std::numbers::sqrt2_v<float>));
    }
    SUBCASE("straight on, the points are square across the line") {
        const auto j = joint(at, {1, 0}, {1, 0}, half, 100.f);
        REQUIRE(j);
        CHECK(tgx::length(j->plus - at) == doctest::Approx(half));
    }
    SUBCASE("turning straight back is not mitered") {
        CHECK_FALSE(joint(at, {1, 0}, {-1, 0}, half, 100.f));
    }
    SUBCASE("a corner sharper than the miter limit is not mitered") {
        const tgx::Vec2 back = tgx::normalize(tgx::Vec2{-1.f, 0.05f});
        CHECK_FALSE(joint(at, {1, 0}, back, half, 100.f));
    }
    SUBCASE("a line too short for its corner is not mitered") {
        const tgx::Vec2 sharp = tgx::normalize(tgx::Vec2{-1.f, 1.f});
        CHECK(joint(at, {1, 0}, sharp, half, 100.f));
        CHECK_FALSE(joint(at, {1, 0}, sharp, half, 1.f));
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

TEST_CASE("fit_into") {
    using tgx::detail::fit_into;
    CHECK(fit_into({4, 2}, {32, 0, 32, 32}) == tgx::Rect{32, 8, 32, 16});   // bars above and below
    CHECK(fit_into({1, 2}, {0, 0, 40, 40}) == tgx::Rect{10, 0, 20, 40});    // left and right
    CHECK(fit_into({16, 9}, {10, 10, 160, 90}) == tgx::Rect{10, 10, 160, 90});
    CHECK(fit_into({}, {0, 0, 10, 10}).empty());
    CHECK(fit_into({4, 2}, {}).empty());
}

TEST_CASE("unit_axis") {
    using tgx::detail::unit_axis;
    CHECK(unit_axis(false, false) == 0.f);
    CHECK(unit_axis(true, false) == -1.f);
    CHECK(unit_axis(false, true) == 1.f);
    CHECK(unit_axis(true, true) == 0.f);
}

TEST_CASE("unit_direction") {
    using tgx::detail::unit_direction;
    CHECK(unit_direction(false, false, false, false) == tgx::Vec2{});
    CHECK(unit_direction(false, true, false, false) == tgx::Vec2{1, 0});
    CHECK(unit_direction(false, false, true, false) == tgx::Vec2{0, -1});
    CHECK(unit_direction(true, true, false, false) == tgx::Vec2{});
    CHECK(tgx::length(unit_direction(false, true, false, true)) == doctest::Approx(1.f));
}
