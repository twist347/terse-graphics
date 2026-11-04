#include "tgx/core/canvas.hpp"

#include "base/utf8.hpp"
#include "canvas/text.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {
    // The codepoints the layout places, including blank glyphs and tabs.
    [[nodiscard]] auto glyphs_of(std::string_view text) -> std::vector<char32_t> {
        std::vector<char32_t> glyphs;
        const tgx::detail::TextLayout layout{tgx::detail::default_font_data(), text, 16};
        layout.for_each([&](const tgx::detail::PositionedGlyph &placed) {
            glyphs.push_back(placed.glyph->codepoint);
        });
        return glyphs;
    }

    [[nodiscard]] constexpr auto glyph(char c) noexcept -> char32_t {
        return static_cast<unsigned char>(c);
    }

    constexpr char32_t space = U' ';
    constexpr char32_t unknown = U'?';
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
    CHECK(encoded(0) == std::string(1, '\0'));
    CHECK(encoded(0x7f) == "\x7f");
    CHECK(encoded(0x80) == "\xc2\x80");
    CHECK(encoded(0x7ff) == "\xdf\xbf");
    CHECK(encoded(0x800) == "\xe0\xa0\x80");
    CHECK(encoded(0xffff) == "\xef\xbf\xbf");
    CHECK(encoded(0x10000) == "\xf0\x90\x80\x80");
    CHECK(encoded(0x10ffff) == "\xf4\x8f\xbf\xbf");
    std::string appended{"prefix"};
    tgx::detail::append_utf8(appended, 0x10000);
    CHECK(appended == "prefix\xf0\x90\x80\x80");
    for (const char32_t c: {U'a', U'\u00e9', U'\u20ac', U'\U0001f600'}) {
        const std::string text = encoded(c);
        CHECK(tgx::detail::sequence_length(static_cast<unsigned char>(text[0])) == text.size());
    }
}

TEST_CASE("TextLayout: one glyph a character") {
    using V = std::vector<char32_t>;
    CHECK(glyphs_of("") == V{});
    CHECK(glyphs_of("a~ ") == V{glyph('a'), glyph('~'), space});
    CHECK(glyphs_of("a\nb") == V{glyph('a'), glyph('b')});
    CHECK(glyphs_of("a\r\nb") == V{glyph('a'), glyph('b')});
    CHECK(glyphs_of("a\rb") == V{glyph('a'), glyph('b')});
    // Missing from the font: a control character, two-, three- and four-byte
    // characters, each one '?'.
    CHECK(glyphs_of("\x01") == V{unknown});
    CHECK(glyphs_of("\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80") == V{unknown, unknown, unknown});
    // A cut-off sequence does not swallow what follows; a stray
    // continuation is a '?' of its own.
    CHECK(glyphs_of("\xe2\x82" "a") == V{unknown, glyph('a')});
    CHECK(glyphs_of("a\xe2\x82") == V{glyph('a'), unknown});
    CHECK(glyphs_of("\x80" "a") == V{unknown, glyph('a')});
    // Overlong controls, surrogates and values past Unicode are not characters.
    CHECK(glyphs_of("\xc0\x8a") == V{unknown});
    CHECK(glyphs_of("\xed\xa0\x80") == V{unknown});
    CHECK(glyphs_of("\xf4\x90\x80\x80") == V{unknown});
}

TEST_CASE("TextLayout: a tab pads to the next stop of four") {
    using V = std::vector<char32_t>;
    CHECK(glyphs_of("\t") == V{space, space, space, space});
    CHECK(glyphs_of("abc\td") == V{glyph('a'), glyph('b'), glyph('c'), space, glyph('d')});
    // A wide character is one column.
    CHECK(glyphs_of("\xc3\xa9\t") == V{unknown, space, space, space});
    // Columns count from the line's start.
    CHECK(glyphs_of("ab\n\t") == V{glyph('a'), glyph('b'), space, space, space, space});
}

TEST_CASE("TextLayout uses font metrics, not the built-in font's") {
    constexpr std::array glyphs{
        tgx::detail::Glyph{.codepoint = U' ', .atlas = {0, 0, 2, 10}, .advance = 2, .blank = true},
        tgx::detail::Glyph{.codepoint = U'?', .atlas = {2, 0, 2, 10}, .advance = 4},
        tgx::detail::Glyph{.codepoint = U'a', .atlas = {4, 0, 4, 8}, .bearing = {1, 2}, .advance = 5},
        tgx::detail::Glyph{.codepoint = U'b', .atlas = {8, 0, 3, 10}, .bearing = {-1, 1}, .advance = 7},
    };
    const tgx::detail::FontData font{
        .atlas_size = {16, 10},
        .line_height = 10,
        .glyphs = glyphs,
        .fallback = 1,
    };
    const tgx::detail::TextLayout layout{font, "ab\na", 20, {100, 50}, {0.5f, 0.5f}};
    CHECK(layout.extent() == tgx::Vec2{24, 40});

    std::vector<tgx::detail::PositionedGlyph> placed;
    layout.for_each([&](const tgx::detail::PositionedGlyph &glyph) { placed.push_back(glyph); });
    REQUIRE(placed.size() == 3);
    CHECK(placed[0].glyph->codepoint == U'a');
    CHECK(placed[0].position == tgx::Vec2{90, 34});
    CHECK(placed[0].size == tgx::Vec2{8, 16});
    CHECK(placed[0].glyph == &glyphs[2]);
    CHECK(placed[1].glyph->codepoint == U'b');
    CHECK(placed[1].position == tgx::Vec2{96, 32});
    CHECK(placed[1].size == tgx::Vec2{6, 20});
    CHECK(placed[2].position == tgx::Vec2{97, 54});

    // A codepoint outside the sparse set uses the font's fallback metrics.
    CHECK(tgx::detail::measure_text(font, "\xc3\xa9", 20) == tgx::Vec2{8, 20});
}

// Measuring needs only the built-in font's widths: no window, no GL.
TEST_CASE("Canvas::measure_text") {
    using tgx::Canvas;
    CHECK(Canvas::default_text_size == tgx::detail::default_font_data().line_height);
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
