#include "tgx/core/error.hpp"
#include "tgx/core/log.hpp"

#include <doctest/doctest.h>

#include <format>

TEST_CASE("errors and log levels print as words") {
    CHECK(std::format("{}", tgx::Error::out_of_memory) == "out of memory");
    CHECK(std::format("{:>6}", tgx::Error::io) == "    io");
    CHECK(std::format("{}", tgx::LogLevel::warn) == "warn");
}

TEST_CASE("the log level reads back as set") {
    const tgx::LogLevel before = tgx::log_level();
    tgx::set_log_level(tgx::LogLevel::debug);
    CHECK(tgx::log_level() == tgx::LogLevel::debug);
    tgx::set_log_level(before);
}
