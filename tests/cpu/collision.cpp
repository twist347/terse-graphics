#include "tgx/core/collision.h"

#include "support.h"

#include <doctest/doctest.h>

#include <array>

TEST_CASE("contains") {
    const tgx::Rect tile{0, 0, 10, 10};
    CHECK(tgx::contains(tile, {0, 0}));
    CHECK(tgx::contains(tile, {9.99f, 5}));
    // Right and bottom edges are outside: a point between two tiles is in one.
    CHECK_FALSE(tgx::contains(tile, {10, 5}));
    CHECK_FALSE(tgx::contains(tile, {5, 10}));

    const tgx::Circle circle{{0, 0}, 5};
    CHECK(tgx::contains(circle, {3, 4}));   // on the edge
    CHECK_FALSE(tgx::contains(circle, {4, 4}));

    CHECK(tgx::contains({0, 0}, {10, 0}, {0, 10}, {2, 2}));
    CHECK(tgx::contains({0, 10}, {10, 0}, {0, 0}, {2, 2}));   // either way round
    CHECK(tgx::contains({0, 0}, {10, 0}, {0, 10}, {5, 0}));   // on an edge
    CHECK_FALSE(tgx::contains({0, 0}, {10, 0}, {0, 10}, {8, 8}));
    // Corners on a line: no inside.
    CHECK_FALSE(tgx::contains({0, 0}, {5, 5}, {10, 10}, {5, 5}));
}

TEST_CASE("contains for a polygon") {
    // An L: not convex, with the notch at the bottom right.
    const std::array l{
        tgx::Vec2{0, 0}, tgx::Vec2{10, 0}, tgx::Vec2{10, 4}, tgx::Vec2{4, 4}, tgx::Vec2{4, 10}, tgx::Vec2{0, 10},
    };
    CHECK(tgx::contains(l, {8, 2}));
    CHECK(tgx::contains(l, {2, 8}));
    CHECK_FALSE(tgx::contains(l, {8, 8}));
    CHECK_FALSE(tgx::contains(l, {-1, 5}));
    // Level with two corners: the ray crosses each edge once.
    CHECK(tgx::contains(l, {2, 4}));
    CHECK_FALSE(tgx::contains(l, {12, 4}));

    // Either way round.
    const std::array reversed{
        tgx::Vec2{0, 10}, tgx::Vec2{4, 10}, tgx::Vec2{4, 4}, tgx::Vec2{10, 4}, tgx::Vec2{10, 0}, tgx::Vec2{0, 0},
    };
    CHECK(tgx::contains(reversed, {8, 2}));
    CHECK_FALSE(tgx::contains(reversed, {8, 8}));

    // Two points have no inside.
    const std::array line{tgx::Vec2{0, 0}, tgx::Vec2{10, 10}};
    CHECK_FALSE(tgx::contains(line, {5, 5}));
}

TEST_CASE("overlaps") {
    CHECK(tgx::overlaps(tgx::Rect{0, 0, 10, 10}, tgx::Rect{5, 5, 10, 10}));
    // Touching edge to edge is not overlapping.
    CHECK_FALSE(tgx::overlaps(tgx::Rect{0, 0, 10, 10}, tgx::Rect{10, 0, 10, 10}));

    CHECK(tgx::overlaps(tgx::Circle{{0, 0}, 5}, tgx::Circle{{8, 0}, 5}));
    CHECK_FALSE(tgx::overlaps(tgx::Circle{{0, 0}, 5}, tgx::Circle{{10, 0}, 5}));

    const tgx::Rect rect{0, 0, 10, 10};
    CHECK(tgx::overlaps(rect, tgx::Circle{{12, 5}, 3}));
    CHECK(tgx::overlaps(tgx::Circle{{12, 5}, 3}, rect));
    // Near the corner, but the corner is further than the radius.
    CHECK_FALSE(tgx::overlaps(rect, tgx::Circle{{13, 13}, 4}));

    // A rect of no area overlaps nothing, even well inside another, as
    // intersection() gives it nothing to share.
    const tgx::Rect none{0, 0, 0, 0};
    CHECK_FALSE(tgx::overlaps(none, tgx::Rect{-1, -1, 2, 2}));
    CHECK_FALSE(tgx::overlaps(tgx::Rect{-1, -1, 2, 2}, none));
    CHECK(tgx::intersection(none, tgx::Rect{-1, -1, 2, 2}).empty());
    CHECK_FALSE(tgx::overlaps(none, tgx::Circle{{0, 0}, 5}));
}

TEST_CASE("closest_point on a segment") {
    CHECK(tgx::closest_point({0, 0}, {10, 0}, {5, 5}) == tgx::Vec2{5, 0});
    // Past either end: the end.
    CHECK(tgx::closest_point({0, 0}, {10, 0}, {-3, 2}) == tgx::Vec2{0, 0});
    CHECK(tgx::closest_point({0, 0}, {10, 0}, {15, 1}) == tgx::Vec2{10, 0});
    // A segment of no length is its one point.
    CHECK(tgx::closest_point({3, 3}, {3, 3}, {7, 9}) == tgx::Vec2{3, 3});
}

TEST_CASE("overlaps for a circle and a segment") {
    CHECK(tgx::overlaps(tgx::Circle{{5, 3}, 4}, {0, 0}, {10, 0}));
    // Touching is not overlapping.
    CHECK_FALSE(tgx::overlaps(tgx::Circle{{5, 3}, 3}, {0, 0}, {10, 0}));
    // Past the end, the end is what counts.
    CHECK(tgx::overlaps(tgx::Circle{{11, 0}, 2}, {0, 0}, {10, 0}));
    CHECK_FALSE(tgx::overlaps(tgx::Circle{{13, 0}, 2}, {0, 0}, {10, 0}));
    // A point near a line.
    CHECK(tgx::overlaps(tgx::Circle{{5, 1}, 2}, {0, 0}, {10, 0}));
}

TEST_CASE("intersection") {
    CHECK(tgx::intersection(tgx::Rect{0, 0, 10, 10}, tgx::Rect{6, 7, 10, 10}) == tgx::Rect{6, 7, 4, 3});
    CHECK(tgx::intersection(tgx::Rect{0, 0, 10, 10}, tgx::Rect{20, 0, 5, 5}) == tgx::Rect{});

    const auto cross = tgx::intersection({0, 0}, {10, 10}, {0, 10}, {10, 0});
    REQUIRE(cross.has_value());
    CHECK(*cross == tgx::Vec2{5, 5});
    // Ends count.
    CHECK(tgx::intersection({0, 0}, {10, 0}, {10, 0}, {10, 10}).has_value());
    CHECK_FALSE(tgx::intersection({0, 0}, {4, 4}, {6, 6}, {10, 0}).has_value());
    // Parallel, even lying along each other: no single point.
    CHECK_FALSE(tgx::intersection({0, 0}, {10, 0}, {0, 1}, {10, 1}).has_value());
    CHECK_FALSE(tgx::intersection({0, 0}, {10, 0}, {5, 0}, {15, 0}).has_value());
}
