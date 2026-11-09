#include "tgx/core/color.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

TEST_CASE("Color from hex") {
    CHECK(tgx::Color::rgba(0x11223344) == tgx::Color{0x11, 0x22, 0x33, 0x44});
    CHECK(tgx::Color::rgb(0x112233) == tgx::Color{0x11, 0x22, 0x33, 0xff});
}

TEST_CASE("named colors are CSS's") {
    CHECK(tgx::colors::light_gray == tgx::Color::rgb(0xc0c0c0));   // silver
    CHECK(tgx::colors::green == tgx::Color::rgb(0x00ff00));        // lime
    CHECK(tgx::colors::maroon == tgx::Color::rgb(0x800000));
    CHECK(tgx::colors::dark_green == tgx::Color::rgb(0x008000));   // CSS green
    CHECK(tgx::colors::navy == tgx::Color::rgb(0x000080));
    CHECK(tgx::colors::olive == tgx::Color::rgb(0x808000));
    CHECK(tgx::colors::teal == tgx::Color::rgb(0x008080));
    CHECK(tgx::colors::purple == tgx::Color::rgb(0x800080));
}

TEST_CASE("Color::hsv") {
    CHECK(tgx::Color::hsv(0, 1, 1) == tgx::colors::red);
    CHECK(tgx::Color::hsv(tgx::radians(120), 1, 1) == tgx::colors::green);
    CHECK(tgx::Color::hsv(tgx::radians(240), 1, 1) == tgx::colors::blue);
    // Hue wraps around the wheel either way.
    CHECK(tgx::Color::hsv(tgx::radians(360), 1, 1) == tgx::colors::red);
    CHECK(tgx::Color::hsv(tgx::radians(-120), 1, 1) == tgx::colors::blue);
    CHECK(tgx::Color::hsv(0, 0, 1) == tgx::colors::white);
    CHECK(tgx::Color::hsv(0, 1, 0) == tgx::colors::black);
    // Fractional channels round to the nearest byte, and s/v are clamped.
    CHECK(tgx::Color::hsv(tgx::radians(30), 1, 1) == tgx::Color{255, 128, 0, 255});
    CHECK(tgx::Color::hsv(0, 0.5f, 0.5f) == tgx::Color{128, 64, 64, 255});
    CHECK(tgx::Color::hsv(0, -1, 2) == tgx::colors::white);
    CHECK(tgx::Color::hsv(0, 2, 1) == tgx::colors::red);
    CHECK(tgx::Color::hsv(0, 1, -1) == tgx::colors::black);
}

TEST_CASE("alpha") {
    CHECK(tgx::colors::red.with_alpha(10) == tgx::Color{255, 0, 0, 10});
    CHECK(tgx::colors::red.fade(0.5f).a == 128);
    // Clamped to [0, 1].
    CHECK(tgx::colors::red.fade(2.f).a == 255);
    CHECK(tgx::colors::red.fade(-1.f).a == 0);
    CHECK(tgx::Color{255, 255, 255, 128}.premultiplied() == tgx::Color{128, 128, 128, 128});
    CHECK(tgx::colors::white.premultiplied() == tgx::colors::white);
    CHECK(tgx::Color{1, 2, 3, 128}.premultiplied() == tgx::Color{1, 1, 2, 128});
    CHECK(tgx::Color{1, 2, 3, 127}.premultiplied() == tgx::Color{0, 1, 1, 127});
    CHECK(tgx::Color{10, 20, 30, 0}.premultiplied() == tgx::colors::transparent);
}

TEST_CASE("Color lerp and to_vec4") {
    CHECK(tgx::lerp(tgx::colors::black, tgx::colors::white, 0.5f) == tgx::Color{128, 128, 128, 255});
    // As with vectors, t goes past the ends; byte channels saturate.
    CHECK(tgx::lerp(tgx::Color{100, 100, 100, 100}, tgx::Color{200, 200, 200, 200}, 1.5f)
          == tgx::Color{250, 250, 250, 250});
    CHECK(tgx::lerp(tgx::Color{100, 100, 100, 100}, tgx::Color{200, 200, 200, 200}, -0.5f)
          == tgx::Color{50, 50, 50, 50});
    CHECK(tgx::lerp(tgx::colors::black, tgx::colors::white, 2.f) == tgx::colors::white);
    CHECK(tgx::lerp(tgx::colors::black, tgx::colors::white, -1.f) == tgx::colors::black);
    CHECK(tgx_test::nearly(tgx::to_vec4(tgx::Color{255, 0, 51, 255}), {1, 0, 0.2f, 1}));
}
