// Timers: once, once_ready and every, with whatever steps a frame rate gives.

#include "tgx/core/timer.hpp"

#include <doctest/doctest.h>

TEST_CASE("once fires one time, after its period") {
    auto timer = tgx::Timer::once(1.f);
    CHECK_FALSE(timer.done());
    CHECK(timer.progress() == 0.f);

    CHECK_FALSE(timer.tick(0.5f));
    CHECK(timer.progress() == doctest::Approx(0.5f));
    CHECK(timer.left() == doctest::Approx(0.5f));

    CHECK(timer.tick(0.6f));
    CHECK(timer.done());
    CHECK(timer.progress() == 1.f);
    CHECK(timer.left() == 0.f);

    CHECK_FALSE(timer.tick(5.f));
    CHECK(timer.done());

    timer.reset();
    CHECK_FALSE(timer.done());
    CHECK(timer.left() == 1.f);
    CHECK(timer.tick(1.f));
}

TEST_CASE("once_ready starts done, and counts from reset") {
    auto reload = tgx::Timer::once_ready(0.2f);
    CHECK(reload.done());
    CHECK(reload.progress() == 1.f);
    CHECK_FALSE(reload.tick(0.1f));

    reload.reset();
    CHECK_FALSE(reload.done());
    CHECK_FALSE(reload.tick(0.1f));
    CHECK(reload.tick(0.1f));
    CHECK(reload.done());
}

TEST_CASE("every keeps its rate at any frame rate") {
    SUBCASE("a frame of a third of the period: every third tick") {
        auto timer = tgx::Timer::every(0.1f);
        int fired = 0;
        for (int i = 0; i < 300; ++i) {
            fired += timer.tick(0.1f / 3.f) ? 1 : 0;
        }
        CHECK(fired == doctest::Approx(100).epsilon(0.02));
    }
    SUBCASE("frames that do not divide the period: the rest carries over") {
        // 0.25 s period at 0.1 s frames: 2.5 frames each, so 40 times in 100.
        auto timer = tgx::Timer::every(0.25f);
        int fired = 0;
        for (int i = 0; i < 100; ++i) {
            fired += timer.tick(0.1f) ? 1 : 0;
        }
        CHECK(fired == 40);
    }
    SUBCASE("a period shorter than a frame: every tick, no backlog") {
        auto timer = tgx::Timer::every(0.01f);
        CHECK(timer.tick(0.5f));
        CHECK(timer.progress() < 1.f);
        CHECK_FALSE(timer.tick(0.f));
    }
    SUBCASE("never done") {
        auto timer = tgx::Timer::every(1.f);
        CHECK(timer.tick(1.f));
        CHECK_FALSE(timer.done());
        CHECK(timer.period() == 1.f);
    }
}

TEST_CASE("a zero step changes nothing") {
    auto timer = tgx::Timer::once(1.f);
    CHECK_FALSE(timer.tick(0.f));
    CHECK(timer.left() == 1.f);
}
