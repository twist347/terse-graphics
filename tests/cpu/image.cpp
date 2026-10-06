#include "tgx/core/image.h"

#include "support.h"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>

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

    tgx::Image image = tgx::Image::create({3, 2}, tgx::colors::blue);
    CHECK(image.size() == tgx::Size{3, 2});
    CHECK(image.pixels().size() == 6);
    CHECK(image.at(2, 1) == tgx::colors::blue);

    image.at(2, 1) = tgx::colors::red;
    // Row by row, width pixels each: (2, 1) is the last.
    CHECK(image.pixels()[5] == tgx::colors::red);

    const std::array pixels{tgx::colors::red, tgx::colors::green};
    const tgx::Image copy = tgx::Image::from_pixels({2, 1}, pixels);
    CHECK(copy.at(1, 0) == tgx::colors::green);
}

TEST_CASE("Image::decode") {
    const auto image = tgx::Image::decode(std::as_bytes(std::span{png}));
    REQUIRE(image.has_value());
    CHECK(image->size() == tgx::Size{1, 2});
    // Rows top to bottom, as the file has them, alpha straight.
    CHECK(image->at(0, 0) == tgx::colors::red);
    CHECK(image->at(0, 1) == tgx::Color{0, 0, 255, 128});

    const std::array garbage{std::byte{1}, std::byte{2}, std::byte{3}};
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
    tgx::Image image = tgx::Image::create({3, 2}, tgx::colors::transparent);
    image.at(0, 0) = tgx::colors::red;
    image.at(2, 1) = tgx::Color{10, 20, 30, 128};

    const tgx_test::TempFile file{"save.png"};
    REQUIRE(image.save(file.path).has_value());
    const auto back = tgx::Image::load(file.path);

    REQUIRE(back.has_value());
    CHECK(back->size() == image.size());
    CHECK(back->at(0, 0) == tgx::colors::red);
    CHECK(back->at(2, 1) == tgx::Color{10, 20, 30, 128});

    const auto nowhere = image.save("there/is/no/such/folder/image.png");
    REQUIRE_FALSE(nowhere.has_value());
    CHECK(nowhere.error() == tgx::Error::io);

#ifdef __linux__
    // Always full: the bytes fit in the stream's buffer and fail only as it
    // closes.
    const auto full = image.save("/dev/full");
    REQUIRE_FALSE(full.has_value());
    CHECK(full.error() == tgx::Error::io);
#endif
}
