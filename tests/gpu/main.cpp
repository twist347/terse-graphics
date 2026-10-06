// The GPU tests' main: makes the one App of the run before any test, as there
// is one window and one Device per process, and fails the run outright when
// there is no display or GL 3.3 to make it with.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "gpu.h"

#include <cstdio>
#include <print>

namespace {
    tgx::App *s_app = nullptr;
}

auto tgx_test::app() -> tgx::App & {
    return *s_app;
}

int main(int argc, char **argv) {
    // The GL info, so a run's log shows which driver it checked.
    tgx::set_log_level(tgx::LogLevel::info);
    auto app = tgx::App::create({.width = 640, .height = 360, .title = "tgx gpu tests", .vsync = false});
    if (!app) {
        std::println(stderr, "tgx gpu tests: no window with a GL 3.3 context ({})", app.error());
        return 1;
    }
    s_app = &*app;
    tgx::set_log_level(tgx::LogLevel::warn);

    doctest::Context context{argc, argv};
    return context.run();
}
