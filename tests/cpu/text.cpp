#include "tgx/canvas.h"

#include "support.h"

#include <doctest/doctest.h>

// Measuring needs only the built-in font's widths: no window, no GL.
TEST_CASE("Canvas::measure_text") {
    using tgx::Canvas;
    // The built-in font is 8x16 a character.
    CHECK(Canvas::measure_text("") == tgx::Vec2{0, 0});
    CHECK(Canvas::measure_text("abc") == tgx::Vec2{24, 16});
    CHECK(Canvas::measure_text("abc", 32) == tgx::Vec2{48, 32});
    CHECK(Canvas::measure_text("ab\ncdef") == tgx::Vec2{32, 32});
    // A '\n' at the end starts a line that counts.
    CHECK(Canvas::measure_text("abc\n") == tgx::Vec2{24, 32});
    // A UTF-8 character the font lacks is one '?' wide, not one per byte.
    CHECK(Canvas::measure_text("\xc3\xa9") == tgx::Vec2{8, 16});
}
