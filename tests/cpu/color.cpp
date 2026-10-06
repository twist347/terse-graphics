#include "tgx/color.h"

#include "support.h"

#include <doctest/doctest.h>

TEST_CASE("Color from hex") {
    CHECK(tgx::Color::rgba(0x11223344) == tgx::Color{0x11, 0x22, 0x33, 0x44});
    CHECK(tgx::Color::rgb(0x112233) == tgx::Color{0x11, 0x22, 0x33, 0xff});
}

TEST_CASE("Color::hsv") {
    CHECK(tgx::Color::hsv(0, 1, 1) == tgx::colors::red);
    CHECK(tgx::Color::hsv(120, 1, 1) == tgx::colors::green);
    CHECK(tgx::Color::hsv(240, 1, 1) == tgx::colors::blue);
    // Hue wraps around the wheel either way.
    CHECK(tgx::Color::hsv(360, 1, 1) == tgx::colors::red);
    CHECK(tgx::Color::hsv(-120, 1, 1) == tgx::colors::blue);
    CHECK(tgx::Color::hsv(0, 0, 1) == tgx::colors::white);
    CHECK(tgx::Color::hsv(0, 1, 0) == tgx::colors::black);
}

TEST_CASE("alpha") {
    CHECK(tgx::colors::red.with_alpha(10) == tgx::Color{255, 0, 0, 10});
    CHECK(tgx::colors::red.fade(0.5f).a == 128);
    // Clamped to [0, 1].
    CHECK(tgx::colors::red.fade(2.f).a == 255);
    CHECK(tgx::colors::red.fade(-1.f).a == 0);
    CHECK(tgx::Color{255, 255, 255, 128}.premultiplied() == tgx::Color{128, 128, 128, 128});
    CHECK(tgx::colors::white.premultiplied() == tgx::colors::white);
}

TEST_CASE("Color lerp and to_vec4") {
    CHECK(tgx::lerp(tgx::colors::black, tgx::colors::white, 0.5f) == tgx::Color{128, 128, 128, 255});
    // Unlike vector lerps, t is clamped: a channel has nowhere to go.
    CHECK(tgx::lerp(tgx::colors::black, tgx::colors::white, 2.f) == tgx::colors::white);
    CHECK(tgx::lerp(tgx::colors::black, tgx::colors::white, -1.f) == tgx::colors::black);
    CHECK(tgx_test::near(tgx::to_vec4(tgx::Color{255, 0, 51, 255}), {1, 0, 0.2f, 1}));
}
