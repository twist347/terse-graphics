#pragma once

#include "core/default_font.hpp"
#include "core/utf8.hpp"

#include <cstddef>
#include <string_view>

// Text into the built-in font's glyphs, apart from GL so tests can use it.
namespace tgx::detail {
    // Glyph index for a line break.
    inline constexpr std::size_t newline_glyph = default_font::glyphs.size();

    // Moves the pen, draws nothing.
    inline constexpr std::size_t space_glyph = ' ' - default_font::first;

    // Columns between tab stops.
    inline constexpr std::size_t tab_columns = 4;

    // Calls f with each character's glyph index, or newline_glyph. One UTF-8
    // sequence is one character, '?' without a glyph. '\t' pads to the next
    // tab stop (monospaced font). '\r' is skipped, so "\r\n" ends a line once.
    template<typename F>
    auto for_each_glyph(std::string_view text, F f) noexcept -> void {
        // Since the start of the line.
        std::size_t column = 0;
        for (std::size_t i = 0; i < text.size();) {
            const auto c = static_cast<unsigned char>(text[i]);
            // Only continuation bytes are taken, so a cut-off sequence does
            // not swallow the next character.
            const std::size_t length = sequence_length(c);
            ++i;
            for (std::size_t taken = 1; taken < length && i < text.size(); ++taken, ++i) {
                if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) {
                    break;
                }
            }

            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                f(newline_glyph);
                column = 0;
                continue;
            }
            if (c == '\t') {
                do {
                    f(space_glyph);
                    ++column;
                } while (column % tab_columns != 0);
                continue;
            }
            const bool printable = c >= static_cast<unsigned char>(default_font::first)
                                   && c <= static_cast<unsigned char>(default_font::last);
            f(static_cast<std::size_t>((printable ? static_cast<char>(c) : '?') - default_font::first));
            ++column;
        }
    }
}
