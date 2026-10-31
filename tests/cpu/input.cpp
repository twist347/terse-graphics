#include "platform/input_internal.hpp"

#include <doctest/doctest.h>

namespace {
    using tgx::Key;
    using tgx::MouseButton;
    using tgx::detail::InputAccess;
    using enum tgx::detail::InputAction;
}

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

TEST_CASE("Input keeps held keys, clearing edges between polls") {
    auto input = InputAccess::create();
    CHECK_FALSE(input.down(Key::a));
    CHECK_FALSE(input.pressed(Key::a));
    CHECK_FALSE(input.released(Key::a));
    CHECK_FALSE(input.repeated(Key::a));

    InputAccess::key(input, Key::a, press);
    CHECK(input.down(Key::a));
    CHECK(input.pressed(Key::a));
    CHECK(input.repeated(Key::a));
    CHECK_FALSE(input.released(Key::a));

    InputAccess::begin_frame(input);
    CHECK(input.down(Key::a));
    CHECK_FALSE(input.pressed(Key::a));
    CHECK_FALSE(input.released(Key::a));
    CHECK_FALSE(input.repeated(Key::a));

    InputAccess::key(input, Key::a, repeat);
    CHECK(input.down(Key::a));
    CHECK(input.repeated(Key::a));
    CHECK_FALSE(input.pressed(Key::a));
    CHECK_FALSE(input.released(Key::a));

    InputAccess::begin_frame(input);
    InputAccess::key(input, Key::a, release);
    CHECK_FALSE(input.down(Key::a));
    CHECK(input.released(Key::a));
    CHECK_FALSE(input.pressed(Key::a));
    CHECK_FALSE(input.repeated(Key::a));

    InputAccess::begin_frame(input);
    CHECK_FALSE(input.down(Key::a));
    CHECK_FALSE(input.released(Key::a));
}

TEST_CASE("Input preserves a tap and a release followed by a press within one poll") {
    auto input = InputAccess::create();
    InputAccess::key(input, Key::space, press);
    InputAccess::key(input, Key::space, release);
    CHECK_FALSE(input.down(Key::space));
    CHECK(input.pressed(Key::space));
    CHECK(input.released(Key::space));
    CHECK(input.repeated(Key::space));

    InputAccess::begin_frame(input);
    InputAccess::key(input, Key::space, press);
    InputAccess::begin_frame(input);
    InputAccess::key(input, Key::space, release);
    InputAccess::key(input, Key::space, press);
    CHECK(input.down(Key::space));
    CHECK(input.pressed(Key::space));
    CHECK(input.released(Key::space));
    CHECK(input.repeated(Key::space));

    // A repeat reports an event, but does not establish a held key.
    InputAccess::key(input, Key::b, repeat);
    CHECK(input.repeated(Key::b));
    CHECK_FALSE(input.down(Key::b));
    CHECK_FALSE(input.pressed(Key::b));

    InputAccess::key(input, Key::unknown, press);
    CHECK_FALSE(input.down(Key::unknown));
    CHECK_FALSE(input.pressed(Key::unknown));
}

TEST_CASE("Input mouse buttons preserve holds and taps too") {
    auto input = InputAccess::create();
    for (const auto button: {MouseButton::left, MouseButton::right, MouseButton::middle, MouseButton::x1, MouseButton::x2}) {
        CHECK_FALSE(input.down(button));
        InputAccess::button(input, button, press);
        CHECK(input.down(button));
        CHECK(input.pressed(button));
        CHECK_FALSE(input.released(button));

        InputAccess::begin_frame(input);
        CHECK(input.down(button));
        CHECK_FALSE(input.pressed(button));
        InputAccess::button(input, button, release);
        CHECK_FALSE(input.down(button));
        CHECK(input.released(button));

        InputAccess::begin_frame(input);
        CHECK_FALSE(input.released(button));
        InputAccess::button(input, button, press);
        InputAccess::button(input, button, release);
        CHECK(input.pressed(button));
        CHECK(input.released(button));
        CHECK_FALSE(input.down(button));
    }
}

TEST_CASE("Input axis and direction read this instance's held keys") {
    auto input = InputAccess::create();
    auto other = InputAccess::create();
    CHECK(input.direction() == tgx::Vec2{});

    InputAccess::key(input, Key::a, press);
    CHECK(input.axis(Key::a, Key::d) == -1.f);
    CHECK(input.direction() == tgx::Vec2{-1, 0});
    CHECK(other.direction() == tgx::Vec2{});
    CHECK_FALSE(other.pressed(Key::a));

    InputAccess::key(input, Key::d, press);
    CHECK(input.axis(Key::a, Key::d) == 0.f);
    CHECK(input.direction() == tgx::Vec2{});
    InputAccess::key(input, Key::a, release);
    InputAccess::key(input, Key::w, press);
    CHECK(input.direction().x == doctest::Approx(0.70710678f));
    CHECK(input.direction().y == doctest::Approx(-0.70710678f));

    InputAccess::key(input, Key::right, press);
    CHECK(tgx::length(input.direction()) == doctest::Approx(1.f));
    CHECK(input.direction(Key::left, Key::right, Key::up, Key::down) == tgx::Vec2{1, 0});

    InputAccess::key(other, Key::left, press);
    CHECK(other.direction() == tgx::Vec2{-1, 0});
    InputAccess::reset(other);
    CHECK(input.down(Key::d));
}

TEST_CASE("Input mouse movement, wheel and UTF-8 text accumulate for one poll") {
    auto input = InputAccess::create();
    InputAccess::reset(input, {10, 20});
    CHECK(input.mouse_delta() == tgx::Vec2{});
    InputAccess::cursor(input, {13, 24});
    InputAccess::cursor(input, {12, 27});
    CHECK(input.mouse() == tgx::Vec2{12, 27});
    CHECK(input.mouse_delta() == tgx::Vec2{2, 7});

    InputAccess::scroll(input, {0.25f, 1.f});
    InputAccess::scroll(input, {0.75f, -0.5f});
    CHECK(input.wheel() == tgx::Vec2{1.f, 0.5f});
    InputAccess::character(input, U'a');
    InputAccess::character(input, U'\u00e9');
    InputAccess::character(input, U'\u20ac');
    InputAccess::character(input, U'\U0001f600');
    CHECK(input.text() == "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80");

    InputAccess::end_frame(input);
    CHECK(input.mouse_delta() == tgx::Vec2{2, 7});
    CHECK_FALSE(input.text().empty());
    InputAccess::begin_frame(input);
    CHECK(input.mouse() == tgx::Vec2{12, 27});
    CHECK(input.mouse_delta() == tgx::Vec2{});
    CHECK(input.wheel() == tgx::Vec2{});
    CHECK(input.text().empty());
    InputAccess::cursor(input, {15, 28});
    CHECK(input.mouse_delta() == tgx::Vec2{3, 1});
}

TEST_CASE("Input cursor settling lasts through the next poll and tracks the new position") {
    auto input = InputAccess::create();
    InputAccess::reset(input, {10, 20});
    InputAccess::cursor(input, {13, 24});
    InputAccess::settle_mouse(input, {100, 200});
    CHECK(input.mouse() == tgx::Vec2{100, 200});
    CHECK(input.mouse_delta() == tgx::Vec2{3, 4});
    InputAccess::cursor(input, {120, 230});
    CHECK(input.mouse_delta() == tgx::Vec2{3, 4});

    InputAccess::begin_frame(input);
    InputAccess::cursor(input, {300, 400});
    InputAccess::cursor(input, {301, 402});
    CHECK(input.mouse() == tgx::Vec2{301, 402});
    CHECK(input.mouse_delta() == tgx::Vec2{});
    InputAccess::end_frame(input);
    CHECK(input.mouse_delta() == tgx::Vec2{});

    InputAccess::begin_frame(input);
    InputAccess::cursor(input, {304, 406});
    CHECK(input.mouse_delta() == tgx::Vec2{3, 4});
}

TEST_CASE("Input reset clears all events, holds and cursor settling") {
    auto input = InputAccess::create();
    InputAccess::key(input, Key::a, press);
    InputAccess::key(input, Key::b, press);
    InputAccess::key(input, Key::b, release);
    InputAccess::button(input, MouseButton::left, press);
    InputAccess::cursor(input, {50, 60});
    InputAccess::scroll(input, {1, 2});
    InputAccess::character(input, U'x');
    InputAccess::settle_mouse(input, {100, 200});

    InputAccess::reset(input, {7, 9});
    CHECK_FALSE(input.down(Key::a));
    CHECK_FALSE(input.pressed(Key::a));
    CHECK_FALSE(input.repeated(Key::a));
    CHECK_FALSE(input.released(Key::b));
    CHECK_FALSE(input.down(MouseButton::left));
    CHECK_FALSE(input.pressed(MouseButton::left));
    CHECK(input.mouse() == tgx::Vec2{7, 9});
    CHECK(input.mouse_delta() == tgx::Vec2{});
    CHECK(input.wheel() == tgx::Vec2{});
    CHECK(input.text().empty());
    InputAccess::cursor(input, {10, 13});
    CHECK(input.mouse_delta() == tgx::Vec2{3, 4});
}
