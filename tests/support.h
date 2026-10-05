#pragma once

// What the tests share: comparing within float rounding, and printing tgx
// values when a check fails.

#include "tgx/color.h"
#include "tgx/math.h"

#include <doctest/doctest.h>

#include <format>
#include <string>

namespace tgx_test {
    // Vectors equal to within float rounding, as trigonometry leaves them.
    [[nodiscard]] inline auto near(tgx::Vec2 a, tgx::Vec2 b) -> bool {
        return a.x == doctest::Approx(b.x).epsilon(1e-5) && a.y == doctest::Approx(b.y).epsilon(1e-5);
    }

    [[nodiscard]] inline auto near(tgx::Vec4 a, tgx::Vec4 b) -> bool {
        return a.x == doctest::Approx(b.x) && a.y == doctest::Approx(b.y)
               && a.z == doctest::Approx(b.z) && a.w == doctest::Approx(b.w);
    }

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
