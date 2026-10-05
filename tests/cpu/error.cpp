#include "tgx/error.h"
#include "tgx/log.h"

#include <doctest/doctest.h>

#include <format>

TEST_CASE("errors and log levels print as words") {
    CHECK(std::format("{}", tgx::Error::out_of_memory) == "out of memory");
    CHECK(std::format("{:>6}", tgx::Error::io) == "    io");
    CHECK(std::format("{}", tgx::LogLevel::warn) == "warn");
}
