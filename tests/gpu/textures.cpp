// Sprites, and textures changing or going while sprites of them wait in the
// batch: each sprite shows the texture as it was when it was added.

#include "gpu.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <numbers>
#include <utility>

namespace {
    // Red, green on top; blue, white below.
    [[nodiscard]] auto quarters() -> tgx::Texture {
        constexpr std::array colors{tgx::colors::red, tgx::colors::green, tgx::colors::blue, tgx::colors::white};
        return tgx_test::texture(tgx::Image::from_pixels({2, 2}, colors), {.filter = tgx::TextureFilter::nearest});
    }

    // A color per texel, for each piece of a nine-slice.
    constexpr std::array nine{
        tgx::colors::red, tgx::colors::green, tgx::colors::blue,
        tgx::colors::yellow, tgx::colors::cyan, tgx::colors::magenta,
        tgx::colors::white, tgx::colors::gray, tgx::colors::orange,
    };

    [[nodiscard]] auto ninths() -> tgx::Texture {
        return tgx_test::texture(tgx::Image::from_pixels({3, 3}, nine), {.filter = tgx::TextureFilter::nearest});
    }
}

TEST_CASE("a sprite the right way up, mirrored, and turned clockwise") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const tgx::Texture texture = quarters();

    canvas.clear(tgx::colors::black);
    canvas.sprite(texture, {.size = {32, 32}});
    tgx::Image image = target.read();
    CHECK(image[8, 8] == tgx::colors::red);
    CHECK(image[24, 8] == tgx::colors::green);
    CHECK(image[8, 24] == tgx::colors::blue);
    CHECK(image[24, 24] == tgx::colors::white);

    canvas.clear(tgx::colors::black);
    canvas.sprite(texture, {.size = {32, 32}, .src = {0, 0, -2, 2}});
    image = target.read();
    CHECK(image[8, 8] == tgx::colors::green);
    CHECK(image[24, 8] == tgx::colors::red);

    canvas.clear(tgx::colors::black);
    canvas.sprite(texture, {
        .position = {32, 32},
        .size = {32, 32},
        .origin = {16, 16},
        .rotation = std::numbers::pi_v<float> / 2.f,
    });
    image = target.read();
    CHECK(image[40, 24] == tgx::colors::red);
    CHECK(image[40, 40] == tgx::colors::green);
    CHECK(image[24, 24] == tgx::colors::blue);
}

TEST_CASE("a part of a texture as src, tinted") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const tgx::Texture texture = quarters();

    canvas.clear(tgx::colors::black);
    canvas.sprite(texture, {.size = {32, 32}, .src = {0, 1, 2, 1}});
    canvas.sprite(texture, {.position = {32, 0}, .size = {32, 32}, .src = {1, 1, 1, 1}, .tint = tgx::colors::red});

    const tgx::Image image = target.read();
    CHECK(image[8, 16] == tgx::colors::blue);
    CHECK(image[24, 16] == tgx::colors::white);
    CHECK(image[48, 16] == tgx::colors::red);
}

TEST_CASE("a nine-slice keeps its corners, stretches its edges and middle") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const tgx::Texture texture = ninths();
    constexpr tgx::Borders borders{1, 1, 1, 1};

    canvas.clear(tgx::colors::black);
    canvas.nine_slice(texture, {.size = {64, 48}, .borders = borders, .border_scale = 8});
    tgx::Image image = target.read();
    // Borders 8 wide, the middle 48 by 32.
    constexpr std::array xs{4, 32, 60};
    constexpr std::array ys{4, 24, 44};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            CAPTURE(row);
            CAPTURE(column);
            CHECK(image[xs[column], ys[row]] == nine[row * 3 + column]);
        }
    }
    CHECK(image[32, 56] == tgx::colors::black);

    // Too small for its borders: they shrink to 4, and no middle is left.
    canvas.clear(tgx::colors::black);
    canvas.nine_slice(texture, {.size = {8, 8}, .borders = borders, .border_scale = 8});
    image = target.read();
    CHECK(image[2, 2] == tgx::colors::red);
    CHECK(image[6, 2] == tgx::colors::blue);
    CHECK(image[2, 6] == tgx::colors::white);
    CHECK(image[6, 6] == tgx::colors::orange);
    CHECK(image[10, 2] == tgx::colors::black);

    // Turned clockwise about its middle: the top-left corner goes top right.
    canvas.clear(tgx::colors::black);
    canvas.nine_slice(texture, {
        .position = {32, 32},
        .size = {64, 48},
        .borders = borders,
        .border_scale = 8,
        .origin = {32, 24},
        .rotation = std::numbers::pi_v<float> / 2.f,
    });
    image = target.read();
    CHECK(image[52, 4] == tgx::colors::red);
    CHECK(image[12, 60] == tgx::colors::orange);
}

TEST_CASE("a nine-slice with zero top and bottom is cut in three") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const tgx::Texture texture = ninths();

    canvas.clear(tgx::colors::black);
    canvas.nine_slice(texture, {.size = {64, 30}, .borders = {1, 0, 1, 0}, .border_scale = 8});

    // Each column keeps all three rows, 10 high.
    const tgx::Image image = target.read();
    CHECK(image[4, 5] == tgx::colors::red);
    CHECK(image[4, 15] == tgx::colors::yellow);
    CHECK(image[32, 15] == tgx::colors::cyan);
    CHECK(image[60, 25] == tgx::colors::orange);
}

TEST_CASE("a texture destroyed while its sprite waits, and one made after it") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    {
        const tgx::Texture doomed = tgx_test::plain(tgx::colors::magenta);
        canvas.sprite(doomed, {.size = {20, 20}});
    }
    // Likely given the GL name the one before had.
    const tgx::Texture after = tgx_test::plain(tgx::colors::cyan);
    canvas.sprite(after, {.position = {20, 0}, .size = {20, 20}});

    const tgx::Image image = target.read();
    CHECK(image[10, 10] == tgx::colors::magenta);
    CHECK(image[30, 10] == tgx::colors::cyan);
}

TEST_CASE("a texture moved and move-assigned while its sprites wait") {
    const tgx::RenderTarget target = tgx_test::target({80, 20});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    {
        tgx::Texture a = tgx_test::plain(tgx::colors::red);
        canvas.sprite(a, {.size = {20, 20}});
        tgx::Texture b = std::move(a);
        canvas.sprite(b, {.position = {20, 0}, .size = {20, 20}});
        tgx::Texture c = tgx_test::plain(tgx::colors::green);
        canvas.sprite(c, {.position = {40, 0}, .size = {20, 20}});
        c = std::move(b);   // c's own texture goes, b's moves into it
        canvas.sprite(c, {.position = {60, 0}, .size = {20, 20}});
    }

    const tgx::Image image = target.read();
    CHECK(image[10, 10] == tgx::colors::red);
    CHECK(image[30, 10] == tgx::colors::red);
    CHECK(image[50, 10] == tgx::colors::green);
    CHECK(image[70, 10] == tgx::colors::red);
}

TEST_CASE("a texture updated while a sprite of it waits") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    tgx::Texture changing = tgx_test::texture(tgx::Image::create({1, 1}, tgx::colors::red), {.access = tgx::TextureAccess::dynamic});

    canvas.clear(tgx::colors::black);
    canvas.sprite(changing, {.size = {20, 20}});
    changing.update(0, 0, tgx::Image::create({1, 1}, tgx::colors::blue));
    canvas.sprite(changing, {.position = {20, 0}, .size = {20, 20}});

    const tgx::Image image = target.read();
    CHECK(image[10, 10] == tgx::colors::red);
    CHECK(image[30, 10] == tgx::colors::blue);
}

TEST_CASE("a texture made by size is dynamic, and filled by update") {
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    auto blank = tgx::Texture::create({2, 2}, {.filter = tgx::TextureFilter::nearest});
    REQUIRE(blank.has_value());
    CHECK(blank->params().access == tgx::TextureAccess::dynamic);
    CHECK(blank->params().filter == tgx::TextureFilter::nearest);

    blank->update(0, 0, tgx::Image::create({2, 2}, tgx::colors::green));
    canvas.sprite(*blank, {.size = {8, 8}});
    CHECK(target.read()[4, 4] == tgx::colors::green);
}

TEST_CASE("Texture::load: a file drawn as saved, a missing one an io error") {
    const tgx_test::TempFile file{"texture.png"};
    REQUIRE(tgx::Image::from_pixels({2, 2}, std::array{
        tgx::colors::red, tgx::colors::green, tgx::colors::blue, tgx::colors::white,
    }).save(file.path).has_value());
    const auto loaded = tgx::Texture::load(file.path, {.filter = tgx::TextureFilter::nearest});
    REQUIRE(loaded.has_value());
    CHECK(loaded->size() == tgx::Size{2, 2});
    CHECK(loaded->params().filter == tgx::TextureFilter::nearest);

    const tgx::RenderTarget target = tgx_test::target({16, 16});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    canvas.sprite(*loaded, {.size = {16, 16}});
    const tgx::Image image = target.read();
    CHECK(image[4, 4] == tgx::colors::red);
    CHECK(image[12, 4] == tgx::colors::green);
    CHECK(image[4, 12] == tgx::colors::blue);
    CHECK(image[12, 12] == tgx::colors::white);

    const auto missing = tgx::Texture::load(tgx_test::temp_path("no_such_texture.png"));
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == tgx::Error::io);
}

TEST_CASE("wrap: a src twice the texture's width, clamped, tiled, mirrored") {
    // Red, green; drawn 8 pixels a texel.
    constexpr std::array two{tgx::colors::red, tgx::colors::green};
    const tgx::RenderTarget target = tgx_test::target({32, 8});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const auto across = [&](tgx::TextureWrap wrap) {
        const tgx::Texture strip = tgx_test::texture(
            tgx::Image::from_pixels({2, 1}, two), {.filter = tgx::TextureFilter::nearest, .wrap = wrap}
        );
        canvas.sprite(strip, {.size = {32, 8}, .src = {0, 0, 4, 1}});
        const tgx::Image image = target.read();
        return std::array{image[4, 4], image[12, 4], image[20, 4], image[28, 4]};
    };
    using tgx::colors::red, tgx::colors::green;

    CHECK(across(tgx::TextureWrap::clamp) == std::array{red, green, green, green});
    CHECK(across(tgx::TextureWrap::repeat) == std::array{red, green, red, green});
    CHECK(across(tgx::TextureWrap::mirror) == std::array{red, green, green, red});
}

TEST_CASE("linear blends neighboring texels, nearest does not") {
    constexpr std::array two{tgx::colors::black, tgx::colors::white};
    const tgx::RenderTarget target = tgx_test::target({64, 16});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    // Texel centers at x = 16 and 48.
    const tgx::Texture smooth = tgx_test::texture(tgx::Image::from_pixels({2, 1}, two));
    canvas.sprite(smooth, {.size = {64, 8}});
    const tgx::Texture sharp = tgx_test::texture(tgx::Image::from_pixels({2, 1}, two), {.filter = tgx::TextureFilter::nearest});
    canvas.sprite(sharp, {.position = {0, 8}, .size = {64, 8}});

    const tgx::Image image = target.read();
    CHECK(image[4, 4] == tgx::colors::black);
    CHECK(tgx_test::near(image[31, 4], {124, 124, 124, 255}, 3));
    CHECK(tgx_test::near(image[40, 4], {195, 195, 195, 255}, 3));
    CHECK(image[60, 4] == tgx::colors::white);
    CHECK(image[31, 12] == tgx::colors::black);
    CHECK(image[32, 12] == tgx::colors::white);
}
