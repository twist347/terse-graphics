#include "tgx/core/canvas.hpp"

#include "core/text.hpp"
#include "core/utf8.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {
    // The glyphs for_each_glyph gives, as indices.
    [[nodiscard]] auto glyphs_of(std::string_view text) -> std::vector<std::size_t> {
        std::vector<std::size_t> glyphs;
        tgx::detail::for_each_glyph(text, [&](std::size_t index) noexcept { glyphs.push_back(index); });
        return glyphs;
    }

    [[nodiscard]] constexpr auto glyph(char c) noexcept -> std::size_t {
        return static_cast<std::size_t>(c - ' ');
    }

    constexpr std::size_t newline = tgx::detail::newline_glyph;
    constexpr std::size_t space = tgx::detail::space_glyph;
    constexpr std::size_t unknown = glyph('?');
}

TEST_CASE("UTF-8 sequence lengths from the lead byte") {
    using tgx::detail::sequence_length;
    CHECK(sequence_length('a') == 1);
    CHECK(sequence_length(0xc3) == 2);
    CHECK(sequence_length(0xe2) == 3);
    CHECK(sequence_length(0xf0) == 4);
    // A stray continuation byte and leads of 5 and more stand alone.
    CHECK(sequence_length(0x80) == 1);
    CHECK(sequence_length(0xf8) == 1);
    CHECK(sequence_length(0xff) == 1);
}

TEST_CASE("UTF-8 out: 1 to 4 bytes, read back by their lead") {
    const auto encoded = [](char32_t c) {
        std::string text{};
        tgx::detail::append_utf8(text, c);
        return text;
    };
    CHECK(encoded(U'a') == "a");
    CHECK(encoded(U'\u00e9') == "\xc3\xa9");
    CHECK(encoded(U'\u20ac') == "\xe2\x82\xac");
    CHECK(encoded(U'\U0001f600') == "\xf0\x9f\x98\x80");
    for (const char32_t c: {U'a', U'\u00e9', U'\u20ac', U'\U0001f600'}) {
        const std::string text = encoded(c);
        CHECK(tgx::detail::sequence_length(static_cast<unsigned char>(text[0])) == text.size());
    }
}

TEST_CASE("for_each_glyph: one glyph a character") {
    using V = std::vector<std::size_t>;
    CHECK(glyphs_of("") == V{});
    CHECK(glyphs_of("a~ ") == V{glyph('a'), glyph('~'), space});
    // Line breaks: "\r\n" is one, a lone '\r' none.
    CHECK(glyphs_of("a\nb") == V{glyph('a'), newline, glyph('b')});
    CHECK(glyphs_of("a\r\nb") == V{glyph('a'), newline, glyph('b')});
    CHECK(glyphs_of("a\rb") == V{glyph('a'), glyph('b')});
    // Missing from the font: a control character, two-, three- and four-byte
    // characters, each one '?'.
    CHECK(glyphs_of("\x01") == V{unknown});
    CHECK(glyphs_of("\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80") == V{unknown, unknown, unknown});
    // A cut-off sequence does not swallow what follows; a stray
    // continuation is a '?' of its own.
    CHECK(glyphs_of("\xe2\x82" "a") == V{unknown, glyph('a')});
    CHECK(glyphs_of("\x80" "a") == V{unknown, glyph('a')});
}

TEST_CASE("for_each_glyph: a tab pads to the next stop of four") {
    using V = std::vector<std::size_t>;
    CHECK(glyphs_of("\t") == V{space, space, space, space});
    CHECK(glyphs_of("abc\td") == V{glyph('a'), glyph('b'), glyph('c'), space, glyph('d')});
    // A wide character is one column.
    CHECK(glyphs_of("\xc3\xa9\t") == V{unknown, space, space, space});
    // Columns count from the line's start.
    CHECK(glyphs_of("ab\n\t") == V{glyph('a'), glyph('b'), newline, space, space, space, space});
}

// Measuring needs only the built-in font's widths: no window, no GL.
TEST_CASE("Canvas::measure_text") {
    using tgx::Canvas;
    // The built-in font is 8x16 a character.
    // Empty text is a line with nothing on it yet, as after a '\n'.
    CHECK(Canvas::measure_text("") == tgx::Vec2{0, 16});
    CHECK(Canvas::measure_text("\n") == tgx::Vec2{0, 32});
    CHECK(Canvas::measure_text("abc") == tgx::Vec2{24, 16});
    CHECK(Canvas::measure_text("abc", 32) == tgx::Vec2{48, 32});
    CHECK(Canvas::measure_text("ab\ncdef") == tgx::Vec2{32, 32});
    // A '\n' at the end starts a line that counts.
    CHECK(Canvas::measure_text("abc\n") == tgx::Vec2{24, 32});
    // A UTF-8 character the font lacks is one '?' wide, not one per byte.
    CHECK(Canvas::measure_text("\xc3\xa9") == tgx::Vec2{8, 16});
    // A tab goes on to the next stop of four columns; '\r' takes no room.
    CHECK(Canvas::measure_text("\t") == tgx::Vec2{32, 16});
    CHECK(Canvas::measure_text("ab\tc") == tgx::Vec2{40, 16});
    CHECK(Canvas::measure_text("abcd\t") == tgx::Vec2{64, 16});
    CHECK(Canvas::measure_text("ab\r\ncd") == tgx::Vec2{16, 32});
}
