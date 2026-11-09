#include "canvas/geometry.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

using tgx_test::nearly;

TEST_CASE("circles get as many segments as keep them round") {
    using tgx::detail::segments_for;
    CHECK(segments_for(0.f) == tgx::detail::min_segments);
    CHECK(segments_for(1e30f) == tgx::detail::max_segments);
    CHECK(segments_for(10.f) <= segments_for(100.f));

    // Enough segments for the tolerance, but one fewer would miss it.
    for (const float radius: {1.f, 10.f, 100.f, 1000.f}) {
        CAPTURE(radius);
        const auto count = segments_for(radius);
        const auto n = static_cast<double>(count);
        const auto error = [radius](double segments) {
            return static_cast<double>(radius) * (1.0 - std::cos(std::numbers::pi / segments));
        };
        CHECK(error(n) <= static_cast<double>(tgx::detail::circle_tolerance));
        if (count > tgx::detail::min_segments) {
            CHECK(error(n - 1.0) > static_cast<double>(tgx::detail::circle_tolerance));
        }
    }
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
        constexpr tgx::detail::Rim rim{{10, 10}, 2.f, tgx::detail::full_circle, 4};
        CHECK(nearly(rim(0), {12, 10}));
        CHECK(nearly(rim(1), {10, 12}));
        CHECK(nearly(rim(4), rim(0)));
    }
}

TEST_CASE("ellipse rims") {
    using tgx::detail::EllipseRim;
    constexpr tgx::Vec2 center{100, 50};

    SUBCASE("points lie on the radii, turned") {
        constexpr EllipseRim rim{center, {40, 10}, {1, 0}, 4};
        CHECK(nearly(rim(0), {140, 50}));
        CHECK(nearly(rim(1), {100, 60}));
        const EllipseRim turned{center, {40, 10}, tgx::from_angle(std::numbers::pi_v<float> / 2.f), 4};
        CHECK(nearly(turned(0), {100, 90}));
    }
    SUBCASE("inset moves the axis points straight in") {
        constexpr EllipseRim rim{center, {40, 10}, {1, 0}, 4, 3.f};
        CHECK(nearly(rim(0), {137, 50}));
        CHECK(nearly(rim(1), {100, 57}));
    }
    SUBCASE("inset past the end curvature keeps each point on its own side") {
        constexpr std::size_t n = 256;
        constexpr EllipseRim edge{center, {40, 4}, {1, 0}, n};
        constexpr EllipseRim inset{center, {40, 4}, {1, 0}, n, 3.f};
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

TEST_CASE("polygon triangulation") {
    using tgx::detail::triangulate_polygon;
    const auto area = [](std::span<const tgx::Vec2> points) {
        float result = 0.f;
        for (std::size_t i = 0; i < points.size(); ++i) {
            result += tgx::cross(points[i], points[(i + 1) % points.size()]);
        }
        return std::abs(result) / 2.f;
    };
    const auto triangles_area = [](std::span<const tgx::Vec2> points, const auto &triangles) {
        float result = 0.f;
        for (const auto &triangle: triangles) {
            const tgx::Vec2 a = points[triangle[0]];
            const tgx::Vec2 b = points[triangle[1]];
            const tgx::Vec2 c = points[triangle[2]];
            result += std::abs(tgx::cross(b - a, c - a)) / 2.f;
        }
        return result;
    };

    SUBCASE("convex, either winding") {
        constexpr std::array square{tgx::Vec2{0, 0}, tgx::Vec2{10, 0}, tgx::Vec2{10, 10}, tgx::Vec2{0, 10}};
        const auto triangles = triangulate_polygon(square);
        REQUIRE(triangles.size() == 2);
        CHECK(triangles_area(square, triangles) == doctest::Approx(area(square)));

        std::array reversed = square;
        std::ranges::reverse(reversed);
        const auto reverse_triangles = triangulate_polygon(reversed);
        REQUIRE(reverse_triangles.size() == 2);
        CHECK(triangles_area(reversed, reverse_triangles) == doctest::Approx(area(reversed)));
    }
    SUBCASE("concave") {
        constexpr std::array u{
            tgx::Vec2{0, 0}, tgx::Vec2{90, 0}, tgx::Vec2{90, 90}, tgx::Vec2{60, 90},
            tgx::Vec2{60, 30}, tgx::Vec2{30, 30}, tgx::Vec2{30, 90}, tgx::Vec2{0, 90},
        };
        const auto triangles = triangulate_polygon(u);
        REQUIRE(triangles.size() == u.size() - 2);
        CHECK(triangles_area(u, triangles) == doctest::Approx(area(u)));
    }
    SUBCASE("duplicates, a closing point and straight-through vertices") {
        constexpr std::array points{
            tgx::Vec2{0, 0}, tgx::Vec2{10, 0}, tgx::Vec2{10, 0}, tgx::Vec2{20, 0},
            tgx::Vec2{20, 10}, tgx::Vec2{20, 10}, tgx::Vec2{0, 10}, tgx::Vec2{0, 0},
        };
        const auto triangles = triangulate_polygon(points);
        REQUIRE(triangles.size() == 2);
        CHECK(triangles_area(points, triangles) == doctest::Approx(200.f));
    }
    SUBCASE("no area") {
        constexpr std::array line{tgx::Vec2{0, 0}, tgx::Vec2{10, 0}, tgx::Vec2{20, 0}};
        CHECK(triangulate_polygon({}).empty());
        CHECK(triangulate_polygon(line).empty());
    }
}

TEST_CASE("corners") {
    SUBCASE("clockwise from the top-left") {
        constexpr auto box = tgx::detail::corners(tgx::Rect{1, 2, 3, 4});
        CHECK(box[0] == tgx::Vec2{1, 2});
        CHECK(box[1] == tgx::Vec2{4, 2});
        CHECK(box[2] == tgx::Vec2{4, 6});
        CHECK(box[3] == tgx::Vec2{1, 6});
    }
    SUBCASE("placed: origin at position, turned around it") {
        const auto box = tgx::detail::placed_corners({4, 2}, {2, 1}, {10, 10}, std::numbers::pi_v<float> / 2.f);
        CHECK(nearly(box[0], {11, 8}));
        CHECK(nearly(box[2], {9, 12}));
    }
    SUBCASE("a rounded rim starts at the left end of the top-left corner") {
        constexpr tgx::Rect rect{0, 0, 100, 50};
        const std::size_t points = tgx::detail::corner_points(32);
        CHECK(points == 9);
        CHECK(nearly(tgx::detail::rounded_rim(rect, 10.f, 0.f, points, 0), {0, 10}));
        CHECK(nearly(tgx::detail::rounded_rim(rect, 10.f, 0.f, points, points - 1), {10, 0}));
        CHECK(nearly(tgx::detail::rounded_rim(rect, 10.f, 4.f, points, 0), {4, 10}));
        // Inset past the radius: a square corner.
        CHECK(nearly(tgx::detail::rounded_rim(rect, 10.f, 12.f, points, 0), {12, 12}));
    }
}

TEST_CASE("line joints") {
    using tgx::detail::joint;
    constexpr tgx::Vec2 at{10, 10};
    constexpr float half = 2.f;

    SUBCASE("a right angle is mitered, its points the diagonal away") {
        const auto j = joint(at, {1, 0}, {0, 1}, half, 100.f);
        REQUIRE(j);
        CHECK(nearly(j->plus + j->minus, at * 2.f));
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
