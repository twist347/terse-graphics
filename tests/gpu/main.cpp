// The GPU tests' main: makes the one App before running tests. Listing tests
// and other doctest queries need no display; running needs GL 3.3.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "gpu.hpp"

#include <cstdio>
#include <print>

namespace {
    tgx::App *s_app = nullptr;
}

auto tgx_test::app() -> tgx::App & {
    return *s_app;
}

int main(int argc, char **argv) {
    doctest::Context context{argc, argv};
    context.setAsDefaultForAssertsOutOfTestCases();
    if (context.shouldExit() || doctest::getContextOptions()->no_run) {
        return context.run();
    }

    // The GL info, so a run's log shows which driver it checked.
    tgx::set_log_level(tgx::LogLevel::info);
    auto app = tgx::App::create({.size = {640, 360}, .title = "tgx gpu tests", .vsync = false});
    if (!app) {
        std::println(stderr, "tgx gpu tests: no window with a GL 3.3 context ({})", app.error());
        return 1;
    }
    s_app = &*app;
    tgx::set_log_level(tgx::LogLevel::warn);

    return context.run();
}
