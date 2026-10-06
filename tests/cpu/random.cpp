#include "tgx/random.h"

#include "support.h"

#include <doctest/doctest.h>

#include <array>
#include <span>
#include <utility>
#include <vector>

TEST_CASE("a seed gives the same numbers everywhere") {
    // PCG32 with tgx's stream, worked out apart from tgx: a platform or a
    // change that drew other numbers from a seed would break saved levels.
    tgx::Random random{42};
    CHECK(random.next_u32() == 3270867926u);
    CHECK(random.next_u32() == 1795671209u);
    CHECK(random.next_u32() == 1924641435u);
    CHECK(random.next_u32() == 1143034755u);

    tgx::Random zero{0};
    CHECK(zero.next_u32() == 3894649422u);
    CHECK(zero.next_u32() == 2055130073u);
}

TEST_CASE("next_float stays in [lo, hi)") {
    tgx::Random random{1};
    for (int i = 0; i < 10000; ++i) {
        const float x = random.next_float(-2.f, 3.f);
        REQUIRE(x >= -2.f);
        REQUIRE(x < 3.f);
    }
}

TEST_CASE("next_int covers both ends and nothing past them") {
    tgx::Random random{2};
    std::array<int, 6> seen{};
    for (int i = 0; i < 6000; ++i) {
        const int roll = random.next_int(1, 6);
        REQUIRE(roll >= 1);
        REQUIRE(roll <= 6);
        ++seen[static_cast<std::size_t>(roll - 1)];
    }
    // Each face about a thousand times.
    for (const int count: seen) {
        CHECK(count > 850);
        CHECK(count < 1150);
    }
    CHECK(random.next_int(5, 5) == 5);
    CHECK(random.next_int(-3, -3) == -3);
}

TEST_CASE("chance, points and directions") {
    tgx::Random random{3};
    CHECK_FALSE(random.chance(0.f));
    CHECK(random.chance(1.f));

    const tgx::Rect rect{10, 20, 30, 40};
    for (int i = 0; i < 1000; ++i) {
        const tgx::Vec2 p = random.point_in(rect);
        REQUIRE(p.x >= rect.x);
        REQUIRE(p.x < rect.right());
        REQUIRE(p.y >= rect.y);
        REQUIRE(p.y < rect.bottom());
        REQUIRE(tgx::length(random.direction()) == doctest::Approx(1.f));
    }
}

namespace {
    template<typename Items>
    concept Pickable = requires(tgx::Random random, Items &&items) { random.pick(std::forward<Items>(items)); };
}

TEST_CASE("pick takes from arrays and vectors alike") {
    tgx::Random random{4};
    const std::array colors{tgx::colors::red, tgx::colors::green, tgx::colors::blue};
    const tgx::Color picked = random.pick(colors);
    CHECK((picked == tgx::colors::red || picked == tgx::colors::green || picked == tgx::colors::blue));

    std::vector<int> items{7};
    random.pick(items) = 8;   // a reference into the vector
    CHECK(items[0] == 8);

    // A temporary container would be gone before its item is used.
    static_assert(!Pickable<std::vector<int>>);
    static_assert(Pickable<std::vector<int> &>);
    static_assert(Pickable<std::span<const int>>);
}
