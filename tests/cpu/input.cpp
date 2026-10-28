#include "platform/input_internal.hpp"

#include <doctest/doctest.h>

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
