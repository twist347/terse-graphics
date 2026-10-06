#include "tgx/core/camera.h"

#include "support.h"

#include <doctest/doctest.h>

using tgx_test::near;

TEST_CASE("the default camera shows the world as it is") {
    const tgx::Camera2D camera;
    CHECK(camera.to_screen({12, 34}) == tgx::Vec2{12, 34});
    CHECK(camera.to_world({12, 34}) == tgx::Vec2{12, 34});
}

TEST_CASE("target shows up at offset, scaled by zoom") {
    const tgx::Camera2D camera{.target = {100, 100}, .offset = {640, 360}, .zoom = 2};
    CHECK(camera.to_screen({100, 100}) == tgx::Vec2{640, 360});
    CHECK(camera.to_screen({110, 100}) == tgx::Vec2{660, 360});
    CHECK(camera.to_world({660, 360}) == tgx::Vec2{110, 100});
}

TEST_CASE("to_world undoes to_screen, and matrix() agrees with it") {
    const tgx::Camera2D camera{.target = {30, -20}, .offset = {400, 300}, .rotation = 0.7f, .zoom = 1.5f};
    for (const tgx::Vec2 p: {tgx::Vec2{0, 0}, tgx::Vec2{123, -45}, tgx::Vec2{-300, 800}}) {
        CHECK(near(camera.to_world(camera.to_screen(p)), p));
        const tgx::Vec4 m = camera.matrix() * tgx::Vec4{p.x, p.y, 0, 1};
        CHECK(near({m.x, m.y}, camera.to_screen(p)));
    }
}
