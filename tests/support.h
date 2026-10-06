#pragma once

// What the tests share: comparing within float or color rounding, files of
// their own, and printing tgx values when a check fails.

#include "tgx/color.h"
#include "tgx/math.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace tgx_test {
    // Vectors equal to within float rounding, as trigonometry leaves them.
    [[nodiscard]] inline auto near(tgx::Vec2 a, tgx::Vec2 b) -> bool {
        return a.x == doctest::Approx(b.x).epsilon(1e-5) && a.y == doctest::Approx(b.y).epsilon(1e-5);
    }

    [[nodiscard]] inline auto near(tgx::Vec4 a, tgx::Vec4 b) -> bool {
        return a.x == doctest::Approx(b.x) && a.y == doctest::Approx(b.y)
               && a.z == doctest::Approx(b.z) && a.w == doctest::Approx(b.w);
    }

    // A path in the temp directory no other run uses: Debug and Release tests
    // running side by side do not trip over each other's files.
    [[nodiscard]] inline auto temp_path(std::string_view name) -> std::filesystem::path {
        static const unsigned run = std::random_device{}();
        return std::filesystem::temp_directory_path() / std::format("tgx_tests_{:08x}_{}", run, name);
    }

    // Colors within tolerance on every channel: drivers round blends and
    // gradients differently.
    [[nodiscard]] inline auto near(tgx::Color a, tgx::Color b, int tolerance) -> bool {
        const auto close = [&](std::uint8_t x, std::uint8_t y) { return std::abs(int{x} - int{y}) <= tolerance; };
        return close(a.r, b.r) && close(a.g, b.g) && close(a.b, b.b) && close(a.a, b.a);
    }

    // A temp_path removed when it goes out of scope, so a failed REQUIRE does
    // not leave the file behind. Declared before whatever holds the file
    // open, as Windows does not remove an open file.
    struct TempFile {
        std::filesystem::path path;

        explicit TempFile(std::string_view name) : path{temp_path(name)} {}
        TempFile(const TempFile &) = delete;
        auto operator=(const TempFile &) -> TempFile & = delete;
        ~TempFile() {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    };

    [[nodiscard]] inline auto string(const std::string &text) -> doctest::String {
        return {text.c_str(), static_cast<doctest::String::size_type>(text.size())};
    }
}

template<>
struct doctest::StringMaker<tgx::Vec2> {
    static auto convert(tgx::Vec2 v) -> String { return tgx_test::string(std::format("{{{}, {}}}", v.x, v.y)); }
};

template<>
struct doctest::StringMaker<tgx::Vec3> {
    static auto convert(tgx::Vec3 v) -> String {
        return tgx_test::string(std::format("{{{}, {}, {}}}", v.x, v.y, v.z));
    }
};

template<>
struct doctest::StringMaker<tgx::Vec4> {
    static auto convert(tgx::Vec4 v) -> String {
        return tgx_test::string(std::format("{{{}, {}, {}, {}}}", v.x, v.y, v.z, v.w));
    }
};

template<>
struct doctest::StringMaker<tgx::Size> {
    static auto convert(tgx::Size s) -> String { return tgx_test::string(std::format("{}x{}", s.width, s.height)); }
};

template<>
struct doctest::StringMaker<tgx::Rect> {
    static auto convert(tgx::Rect r) -> String {
        return tgx_test::string(std::format("{{{}, {}, {}, {}}}", r.x, r.y, r.width, r.height));
    }
};

template<>
struct doctest::StringMaker<tgx::Color> {
    static auto convert(tgx::Color c) -> String {
        return tgx_test::string(std::format("rgba({}, {}, {}, {})", c.r, c.g, c.b, c.a));
    }
};
