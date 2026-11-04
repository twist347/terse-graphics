#include "tgx/core/image.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

// A const Image's pixels are read-only.
static_assert(std::is_same_v<decltype(std::declval<tgx::Image &>().pixels()), std::span<tgx::Color>>);
static_assert(std::is_same_v<decltype(std::declval<const tgx::Image &>().pixels()), std::span<const tgx::Color>>);

namespace {
    // A 1x2 PNG: red on top, half see-through blue below it.
    constexpr std::array<unsigned char, 75> png{
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x99, 0x81, 0xb6, 0x27, 0x00, 0x00, 0x00,
        0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
        0x9f, 0x81, 0x81, 0xe1, 0x7f, 0x03, 0x00, 0x11, 0x79, 0x03, 0x7e, 0x45,
        0x15, 0xcb, 0x06, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
        0x42, 0x60, 0x82,
    };
}

TEST_CASE("Image made in code") {
    CHECK(tgx::Image{}.empty());

    auto image = tgx::Image::create({3, 2}, tgx::colors::blue);
    CHECK(image.size() == tgx::Size{3, 2});
    CHECK(image.pixels().size() == 6);
    CHECK(image[2, 1] == tgx::colors::blue);

    image[2, 1] = tgx::colors::red;
    // Row by row, width pixels each: (2, 1) is the last.
    CHECK(image.pixels()[5] == tgx::colors::red);

    constexpr std::array pixels{tgx::colors::red, tgx::colors::green};
    const auto copy = tgx::Image::from_pixels({2, 1}, pixels);
    CHECK(copy[1, 0] == tgx::colors::green);
}

TEST_CASE("Image::crop copies a whole-pixel rectangle") {
    const auto image = tgx::Image::from_pixels({3, 2}, std::array{
        tgx::colors::red, tgx::colors::green, tgx::colors::blue,
        tgx::colors::cyan, tgx::colors::magenta, tgx::colors::yellow,
    });
    const auto cropped = image.crop(1, 0, {2, 2});
    CHECK(cropped.size() == tgx::Size{2, 2});
    CHECK(std::ranges::equal(cropped.pixels(), std::array{
        tgx::colors::green, tgx::colors::blue, tgx::colors::magenta, tgx::colors::yellow,
    }));
    CHECK(image.crop(3, 2, {0, 0}).empty());
}

TEST_CASE("Image::resize nearest keeps blocks of source pixels") {
    const auto image = tgx::Image::from_pixels({2, 2}, std::array{
        tgx::colors::red, tgx::colors::green, tgx::colors::blue, tgx::colors::white,
    });
    const auto resized = image.resize({4, 4}, tgx::ImageFilter::nearest);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const tgx::Color expected = y < 2
                ? (x < 2 ? tgx::colors::red : tgx::colors::green)
                : (x < 2 ? tgx::colors::blue : tgx::colors::white);
            CHECK(resized[x, y] == expected);
        }
    }
    CHECK(image.resize({1, 1}, tgx::ImageFilter::nearest)[0, 0] == tgx::colors::white);
    CHECK(image.resize({}).empty());
}

TEST_CASE("Image::resize linear interpolates straight RGBA at pixel centers") {
    const auto image = tgx::Image::from_pixels({2, 2}, std::array{
        tgx::colors::red, tgx::colors::green, tgx::colors::blue, tgx::colors::white,
    });
    const auto resized = image.resize({3, 3});
    CHECK(resized[0, 0] == tgx::colors::red);
    CHECK(resized[2, 0] == tgx::colors::green);
    CHECK(resized[0, 2] == tgx::colors::blue);
    CHECK(resized[2, 2] == tgx::colors::white);
    CHECK(resized[1, 0] == tgx::Color{128, 128, 0, 255});
    CHECK(resized[0, 1] == tgx::Color{128, 0, 128, 255});
    CHECK(resized[1, 1] == tgx::Color{128, 128, 128, 255});

    const auto alpha = tgx::Image::from_pixels({1, 2}, std::array{
        tgx::Color{255, 0, 0, 0}, tgx::colors::blue,
    }).resize({3, 1});
    CHECK(alpha[1, 0] == tgx::Color{128, 0, 128, 128});

    const auto empty = tgx::Image{}.resize({2, 2});
    CHECK(empty.size() == tgx::Size{2, 2});
    CHECK(empty[1, 1] == tgx::colors::transparent);
}

TEST_CASE("Image flips mirror pixels in place") {
    const auto source = tgx::Image::from_pixels({3, 2}, std::array{
        tgx::colors::red, tgx::colors::green, tgx::colors::blue,
        tgx::colors::cyan, tgx::colors::magenta, tgx::colors::yellow,
    });

    auto horizontal = source;
    horizontal.flip_horizontal();
    CHECK(std::ranges::equal(horizontal.pixels(), std::array{
        tgx::colors::blue, tgx::colors::green, tgx::colors::red,
        tgx::colors::yellow, tgx::colors::magenta, tgx::colors::cyan,
    }));
    horizontal.flip_horizontal();
    CHECK(std::ranges::equal(horizontal.pixels(), source.pixels()));

    auto vertical = source;
    vertical.flip_vertical();
    CHECK(std::ranges::equal(vertical.pixels(), std::array{
        tgx::colors::cyan, tgx::colors::magenta, tgx::colors::yellow,
        tgx::colors::red, tgx::colors::green, tgx::colors::blue,
    }));
    vertical.flip_vertical();
    CHECK(std::ranges::equal(vertical.pixels(), source.pixels()));
}

TEST_CASE("Image::decode") {
    const auto image = tgx::Image::decode(std::as_bytes(std::span{png}));
    REQUIRE(image.has_value());
    CHECK(image->size() == tgx::Size{1, 2});
    // Rows top to bottom, as the file has them, alpha straight.
    CHECK((*image)[0, 0] == tgx::colors::red);
    CHECK((*image)[0, 1] == tgx::Color{0, 0, 255, 128});

    constexpr std::array garbage{std::byte{1}, std::byte{2}, std::byte{3}};
    const auto bad = tgx::Image::decode(garbage);
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error() == tgx::Error::decode);
}

TEST_CASE("Image::load failures") {
    const auto missing = tgx::Image::load("there/is/no/such/image.png");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == tgx::Error::io);

    // A directory opens on some systems; it is still no file to read.
    const auto directory = tgx::Image::load(std::filesystem::temp_directory_path());
    REQUIRE_FALSE(directory.has_value());
    CHECK(directory.error() == tgx::Error::io);
}

TEST_CASE("Image::save writes what Image::load reads back") {
    auto image = tgx::Image::create({3, 2}, tgx::colors::transparent);
    image[0, 0] = tgx::colors::red;
    image[2, 1] = tgx::Color{10, 20, 30, 128};

    const tgx_test::TempFile file{"save.png"};
    REQUIRE(image.save(file.path).has_value());
    const auto back = tgx::Image::load(file.path);

    REQUIRE(back.has_value());
    CHECK(back->size() == image.size());
    CHECK((*back)[0, 0] == tgx::colors::red);
    CHECK((*back)[2, 1] == tgx::Color{10, 20, 30, 128});

    const auto nowhere = image.save("there/is/no/such/folder/image.png");
    REQUIRE_FALSE(nowhere.has_value());
    CHECK(nowhere.error() == tgx::Error::io);

}

TEST_CASE("Image::save leaves an existing partial-looking sibling alone") {
    const tgx_test::TempFile directory{"save-partial"};
    std::filesystem::create_directory(directory.path);
    const std::filesystem::path file = directory.path / "save.png";
    std::filesystem::path sibling = file;
    sibling += ".tgx-partial";
    {
        std::ofstream out{sibling, std::ios::binary};
        out << "keep";
    }

    REQUIRE(tgx::Image::create({2, 2}, tgx::colors::red).save(file).has_value());
    std::ifstream in{sibling, std::ios::binary};
    std::string contents{};
    in >> contents;
    CHECK(contents == "keep");

    std::size_t entries = 0;
    for (const auto &entry: std::filesystem::directory_iterator{directory.path}) {
        CHECK(entry.is_regular_file());
        ++entries;
    }
    CHECK(entries == 2);
}

TEST_CASE("concurrent Image::save calls use separate partial files") {
    const tgx_test::TempFile directory{"save-concurrent"};
    std::filesystem::create_directory(directory.path);
    const std::filesystem::path file = directory.path / "save.png";
    const auto red = tgx::Image::create({2, 2}, tgx::colors::red);
    const auto blue = tgx::Image::create({2, 2}, tgx::colors::blue);
    std::array saved{true, true};
    const auto save = [&](const tgx::Image &image, bool &success) {
        for (int i = 0; i < 24; ++i) {
            if (!image.save(file)) {
                success = false;
                return;
            }
        }
    };
    std::thread first{[&] { save(red, saved[0]); }};
    std::thread second{[&] { save(blue, saved[1]); }};
    first.join();
    second.join();

    CHECK(saved[0]);
    CHECK(saved[1]);
    const auto back = tgx::Image::load(file);
    REQUIRE(back.has_value());
    const tgx::Color pixel = (*back)[1, 1];
    CHECK((pixel == tgx::colors::red || pixel == tgx::colors::blue));

    std::size_t entries = 0;
    for (const auto &entry: std::filesystem::directory_iterator{directory.path}) {
        CHECK(entry.is_regular_file());
        ++entries;
    }
    CHECK(entries == 1);
}
