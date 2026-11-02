#include "tgx/core/camera.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <numbers>

using tgx_test::nearly;

TEST_CASE("the default camera shows the world as it is") {
    constexpr tgx::Camera2D camera{};
    CHECK(camera.to_screen({12, 34}) == tgx::Vec2{12, 34});
    CHECK(camera.to_world({12, 34}) == tgx::Vec2{12, 34});
}

TEST_CASE("target shows up at offset, scaled by zoom") {
    constexpr tgx::Camera2D camera{.target = {100, 100}, .offset = {640, 360}, .zoom = 2};
    CHECK(camera.to_screen({100, 100}) == tgx::Vec2{640, 360});
    CHECK(camera.to_screen({110, 100}) == tgx::Vec2{660, 360});
    CHECK(camera.to_world({660, 360}) == tgx::Vec2{110, 100});
}

TEST_CASE("to_world undoes to_screen, and matrix() agrees with it") {
    constexpr tgx::Camera2D camera{.target = {30, -20}, .offset = {400, 300}, .rotation = 0.7f, .zoom = 1.5f};
    for (const tgx::Vec2 p: {tgx::Vec2{0, 0}, tgx::Vec2{123, -45}, tgx::Vec2{-300, 800}}) {
        CHECK(nearly(camera.to_world(camera.to_screen(p)), p));
        const tgx::Vec4 m = camera.matrix() * tgx::Vec4{p.x, p.y, 0, 1};
        CHECK(nearly({m.x, m.y}, camera.to_screen(p)));
    }
}

TEST_CASE("camera rotation is clockwise on the screen") {
    const tgx::Camera2D clockwise{.target = {10, 20}, .offset = {100, 200}, .rotation = std::numbers::pi_v<float> / 2.f, .zoom = 2};
    CHECK(nearly(clockwise.to_screen({13, 20}), {100, 206}));
    CHECK(nearly(clockwise.to_screen({10, 23}), {94, 200}));
    CHECK(nearly(clockwise.to_world({100, 206}), {13, 20}));
    CHECK(nearly(clockwise.matrix() * tgx::Vec4{13, 20, 0, 1}, {100, 206, 0, 1}));

    auto counterclockwise = clockwise;
    counterclockwise.rotation = -clockwise.rotation;
    CHECK(nearly(counterclockwise.to_screen({13, 20}), {100, 194}));
    CHECK(nearly(counterclockwise.to_world({100, 194}), {13, 20}));
    CHECK(nearly(counterclockwise.matrix() * tgx::Vec4{13, 20, 0, 1}, {100, 194, 0, 1}));
}
