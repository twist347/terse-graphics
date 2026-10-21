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
    CHECK(span_of({320, 180}, tgx::Rect{0, 0, 100, 100}, {1280, 720}) == tgx::Size{320, 180});
    CHECK(span_of({}, tgx::Rect{10, 10, 199.6f, 150.2f}, {1280, 720}) == tgx::Size{200, 150});
    CHECK(span_of({}, std::nullopt, {1280, 720}) == tgx::Size{1280, 720});
    CHECK(span_of({}, tgx::Rect{}, {1280, 720}).empty());
    // Too large for an int: kept within one.
    CHECK(span_of({}, tgx::Rect{0, 0, 1e30f, 1e30f}, {1280, 720}).width > 0);
}

TEST_CASE("the part shown: the viewport, or all; a size of its own fit in that") {
    using tgx::detail::shown;
    const tgx::detail::Surface surface{.pixels = {200, 100}, .units = {100, 50}};
    CHECK(shown({}, std::nullopt, surface) == tgx::Rect{0, 0, 100, 50});
    CHECK(shown({}, tgx::Rect{10, 10, 20, 20}, surface) == tgx::Rect{10, 10, 20, 20});
    CHECK(shown({}, tgx::Rect{}, surface).empty());
    CHECK(shown({4, 1}, std::nullopt, surface) == tgx::Rect{0, 12.5f, 100, 25});
    CHECK(shown({4, 1}, tgx::Rect{}, surface).empty());
}

TEST_CASE("a viewport in pixels: edges rounded, a huge one kept within int") {
    using tgx::detail::pixel_viewport;
    using V = tgx::gl::Viewport;
    const tgx::detail::Surface hidpi{.pixels = {200, 100}, .units = {100, 50}};
    CHECK(pixel_viewport({10.2f, 5, 30, 20}, hidpi) == V{20, 10, 60, 40});   // edges 20.4 and 80.4
    CHECK(pixel_viewport({0, 0, 100, 50}, hidpi) == V{0, 0, 200, 100});
    CHECK(pixel_viewport({}, hidpi) == V{});
    // Nothing to scale by: all of it.
    CHECK(pixel_viewport({1, 1, 5, 5}, {.pixels = {}, .units = {}}) == V{});
    const V huge = pixel_viewport({0, 0, 1e30f, 1e30f}, hidpi);
    CHECK(huge.x == 0);
    CHECK(huge.width > 0);
    CHECK(huge.height > 0);
}

TEST_CASE("a clip rect in pixels: scaled, rounded, kept within what is shown") {
    using tgx::detail::pixel_clip;
    using V = tgx::gl::Viewport;
    // 1:1, shown from (10, 20).
    CHECK(pixel_clip({4, 6, 30, 40}, {100, 100}, {10, 20, 100, 100}) == V{14, 26, 30, 40});
    // A 4x2 canvas shown 32x16: 8 pixels a unit.
    CHECK(pixel_clip({2, 0, 2, 1}, {4, 2}, {32, 8, 32, 16}) == V{48, 8, 16, 8});
    // Edges rounded, not the size: 0.4..2.6 units at 10 pixels a unit.
    CHECK(pixel_clip({0.04f, 0, 0.22f, 1}, {1, 1}, {0, 0, 10, 10}) == V{0, 0, 3, 10});
    // Cut to what is shown, on every side.
    CHECK(pixel_clip({-50, -50, 80, 80}, {100, 100}, {10, 20, 100, 100}) == V{10, 20, 30, 30});
    CHECK(pixel_clip({90, 90, 50, 50}, {100, 100}, {0, 0, 100, 100}) == V{90, 90, 10, 10});
    // Empty: clips everything away rather than nothing.
    CHECK(pixel_clip({5, 5, 0, 10}, {100, 100}, {0, 0, 100, 100}) == V{5, 5, 0, 10});
    CHECK(pixel_clip({200, 200, 10, 10}, {100, 100}, {0, 0, 100, 100}) == V{100, 100, 0, 0});
    CHECK(pixel_clip({1e30f, 0, 1e30f, 10}, {100, 100}, {0, 0, 100, 100}) == V{100, 0, 0, 10});
    CHECK(pixel_clip({0, 0, 10, 10}, {}, {3, 4, 0, 0}) == V{3, 4, 0, 0});
}
