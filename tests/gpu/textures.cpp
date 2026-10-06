// Sprites, and textures changing or going while sprites of them wait in the
// batch: each sprite shows the texture as it was when it was added.

#include "gpu.h"

#include <doctest/doctest.h>

#include <array>
#include <numbers>
#include <utility>

namespace {
    [[nodiscard]] auto texture(const tgx::Image &image, const tgx::TextureParams &params = {}) -> tgx::Texture {
        auto texture = tgx::Texture::create(image, params);
        REQUIRE(texture.has_value());
        return std::move(*texture);
    }

    [[nodiscard]] auto plain(tgx::Color color) -> tgx::Texture {
        return texture(tgx::Image::create({1, 1}, color));
    }

    // Red, green on top; blue, white below.
    [[nodiscard]] auto quarters() -> tgx::Texture {
        const std::array colors{tgx::colors::red, tgx::colors::green, tgx::colors::blue, tgx::colors::white};
        return texture(tgx::Image::from_pixels({2, 2}, colors), {.filter = tgx::TextureFilter::nearest});
    }
}

TEST_CASE("a sprite the right way up, mirrored, and turned clockwise") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const tgx::Texture texture = quarters();

    canvas.clear(tgx::colors::black);
    canvas.sprite(texture, {.size = {32, 32}});
    tgx::Image image = target.read();
    CHECK(image.at(8, 8) == tgx::colors::red);
    CHECK(image.at(24, 8) == tgx::colors::green);
    CHECK(image.at(8, 24) == tgx::colors::blue);
    CHECK(image.at(24, 24) == tgx::colors::white);

    canvas.clear(tgx::colors::black);
    canvas.sprite(texture, {.size = {32, 32}, .src = {0, 0, -2, 2}});
    image = target.read();
    CHECK(image.at(8, 8) == tgx::colors::green);
    CHECK(image.at(24, 8) == tgx::colors::red);

    canvas.clear(tgx::colors::black);
    canvas.sprite(texture, {
        .position = {32, 32},
        .size = {32, 32},
        .origin = {16, 16},
        .rotation = std::numbers::pi_v<float> / 2.f,
    });
    image = target.read();
    CHECK(image.at(40, 24) == tgx::colors::red);
    CHECK(image.at(40, 40) == tgx::colors::green);
    CHECK(image.at(24, 24) == tgx::colors::blue);
}

TEST_CASE("a part of a texture as src, tinted") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const tgx::Texture texture = quarters();

    canvas.clear(tgx::colors::black);
    canvas.sprite(texture, {.size = {32, 32}, .src = {0, 1, 2, 1}});
    canvas.sprite(texture, {.position = {32, 0}, .size = {32, 32}, .src = {1, 1, 1, 1}, .tint = tgx::colors::red});

    const tgx::Image image = target.read();
    CHECK(image.at(8, 16) == tgx::colors::blue);
    CHECK(image.at(24, 16) == tgx::colors::white);
    CHECK(image.at(48, 16) == tgx::colors::red);
}

TEST_CASE("a texture destroyed while its sprite waits, and one made after it") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    {
        const tgx::Texture doomed = plain(tgx::colors::magenta);
        canvas.sprite(doomed, {.size = {20, 20}});
    }
    // Likely given the GL name the one before had.
    const tgx::Texture after = plain(tgx::colors::cyan);
    canvas.sprite(after, {.position = {20, 0}, .size = {20, 20}});

    const tgx::Image image = target.read();
    CHECK(image.at(10, 10) == tgx::colors::magenta);
    CHECK(image.at(30, 10) == tgx::colors::cyan);
}

TEST_CASE("a texture moved and move-assigned while its sprites wait") {
    const tgx::RenderTarget target = tgx_test::target({80, 20});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    {
        tgx::Texture a = plain(tgx::colors::red);
        canvas.sprite(a, {.size = {20, 20}});
        tgx::Texture b = std::move(a);
        canvas.sprite(b, {.position = {20, 0}, .size = {20, 20}});
        tgx::Texture c = plain(tgx::colors::green);
        canvas.sprite(c, {.position = {40, 0}, .size = {20, 20}});
        c = std::move(b);   // c's own texture goes, b's moves into it
        canvas.sprite(c, {.position = {60, 0}, .size = {20, 20}});
    }

    const tgx::Image image = target.read();
    CHECK(image.at(10, 10) == tgx::colors::red);
    CHECK(image.at(30, 10) == tgx::colors::red);
    CHECK(image.at(50, 10) == tgx::colors::green);
    CHECK(image.at(70, 10) == tgx::colors::red);
}

TEST_CASE("a texture updated while a sprite of it waits") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    tgx::Texture changing = texture(tgx::Image::create({1, 1}, tgx::colors::red), {.access = tgx::TextureAccess::dynamic});

    canvas.clear(tgx::colors::black);
    canvas.sprite(changing, {.size = {20, 20}});
    changing.update(0, 0, tgx::Image::create({1, 1}, tgx::colors::blue));
    canvas.sprite(changing, {.position = {20, 0}, .size = {20, 20}});

    const tgx::Image image = target.read();
    CHECK(image.at(10, 10) == tgx::colors::red);
    CHECK(image.at(30, 10) == tgx::colors::blue);
}
