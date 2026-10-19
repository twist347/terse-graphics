#include "core/viewport.hpp"

#include <doctest/doctest.h>

TEST_CASE("fit_into") {
    using tgx::detail::fit_into;
    CHECK(fit_into({4, 2}, {32, 0, 32, 32}) == tgx::Rect{32, 8, 32, 16});   // bars above and below
    CHECK(fit_into({1, 2}, {0, 0, 40, 40}) == tgx::Rect{10, 0, 20, 40});    // left and right
    CHECK(fit_into({16, 9}, {10, 10, 160, 90}) == tgx::Rect{10, 10, 160, 90});
    CHECK(fit_into({}, {0, 0, 10, 10}).empty());
    CHECK(fit_into({4, 2}, {}).empty());
}

TEST_CASE("a canvas spans its own size, else its viewport, else the surface") {
    using tgx::detail::span_of;
    CHECK(span_of({320, 180}, {0, 0, 100, 100}, {1280, 720}) == tgx::Size{320, 180});
    CHECK(span_of({}, {10, 10, 199.6f, 150.2f}, {1280, 720}) == tgx::Size{200, 150});
    CHECK(span_of({}, {}, {1280, 720}) == tgx::Size{1280, 720});
}
