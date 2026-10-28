#pragma once

#include <bit>
#include <cstddef>
#include <string>

// UTF-8 both ways: typed text in, text drawn out. Apart from GL and GLFW so
// tests can use it.
namespace tgx::detail {
    // Leading one bits. 1 for ASCII, a stray continuation or an invalid lead.
    [[nodiscard]] constexpr auto sequence_length(unsigned char lead) noexcept -> std::size_t {
        const int ones = std::countl_one(lead);
        return ones >= 2 && ones <= 4 ? static_cast<std::size_t>(ones) : 1;
    }

    // Appends the code point as UTF-8, in 1 to 4 bytes.
    inline auto append_utf8(std::string &text, char32_t c) -> void {
        const auto byte = [](char32_t b) noexcept { return static_cast<char>(b); };
        if (c < 0x80) {
            text += byte(c);
        } else if (c < 0x800) {
            text += byte(0xc0 | (c >> 6));
            text += byte(0x80 | (c & 0x3f));
        } else if (c < 0x10000) {
            text += byte(0xe0 | (c >> 12));
            text += byte(0x80 | ((c >> 6) & 0x3f));
            text += byte(0x80 | (c & 0x3f));
        } else {
            text += byte(0xf0 | (c >> 18));
            text += byte(0x80 | ((c >> 12) & 0x3f));
            text += byte(0x80 | ((c >> 6) & 0x3f));
            text += byte(0x80 | (c & 0x3f));
        }
    }
}
