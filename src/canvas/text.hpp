#pragma once

#include "base/utf8.hpp"
#include "canvas/font.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

// Text metrics and placement, apart from GL and any particular font.
namespace tgx::detail {
    inline constexpr std::size_t tab_columns = 4;

    // One UTF-8 sequence is one character. A cut-off sequence does not swallow
    // the next character. '\r' is skipped, so "\r\n" ends a line once.
    template<typename GlyphFn, typename NewlineFn>
    auto scan_text(const FontData &font, std::string_view text, GlyphFn glyph_fn, NewlineFn newline_fn) noexcept -> void {
        std::size_t column = 0;
        for (std::size_t i = 0; i < text.size();) {
            const auto lead = static_cast<unsigned char>(text[i]);
            const std::size_t length = sequence_length(lead);
            ++i;

            char32_t codepoint = lead;
            if (length > 1) {
                codepoint = lead & (0x7f >> length);
                bool valid = true;
                std::size_t taken = 1;
                for (; taken < length && i < text.size(); ++taken, ++i) {
                    const auto next = static_cast<unsigned char>(text[i]);
                    if ((next & 0xc0) != 0x80) {
                        valid = false;
                        break;
                    }
                    codepoint = (codepoint << 6) | (next & 0x3f);
                }
                const char32_t shortest = length == 2 ? 0x80 : length == 3 ? 0x800 : 0x10000;
                if (!valid || taken < length || codepoint < shortest || codepoint > 0x10ffff
                    || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
                    codepoint = U'?';
                }
            }

            if (codepoint == U'\r') {
                continue;
            }
            if (codepoint == U'\n') {
                newline_fn();
                column = 0;
                continue;
            }
            if (codepoint == U'\t') {
                do {
                    glyph_fn(font.glyph(U' '));
                    ++column;
                } while (column % tab_columns != 0);
                continue;
            }
            glyph_fn(font.glyph(codepoint));
            ++column;
        }
    }

    [[nodiscard]] inline auto measure_text(const FontData &font, std::string_view text, float size) noexcept -> Vec2 {
        const float scale = size / font.line_height;
        float widest = 0.f;
        float line = 0.f;
        float lines = 1.f;
        scan_text(
            font, text,
            [&](const Glyph &glyph) noexcept {
                line += glyph.advance * scale;
                widest = std::max(widest, line);
            },
            [&] noexcept {
                line = 0.f;
                lines += 1.f;
            }
        );
        return {widest, lines * size};
    }

    struct PositionedGlyph {
        Vec2 position{};
        Vec2 size{};
        const Glyph *glyph{};
    };

    class TextLayout {
    public:
        TextLayout(
            const FontData &font,
            std::string_view text,
            float size,
            Vec2 position = {},
            Vec2 anchor = {}
        ) noexcept
            : font_{&font}, text_{text}, size_{size}, position_{position}, anchor_{anchor} {
        }

        [[nodiscard]] auto extent() const noexcept -> Vec2 {
            if (!measured_) {
                extent_ = measure_text(*font_, text_, size_);
                measured_ = true;
            }
            return extent_;
        }

        // Blank glyphs are included so callers can inspect advances and tabs.
        template<typename F>
        auto for_each(F f) const noexcept -> void {
            const float scale = size_ / font_->line_height;
            const Vec2 full_extent = anchor_ != Vec2{} ? extent() : Vec2{};
            float y = position_.y - std::round(full_extent.y * anchor_.y);
            std::size_t begin = 0;
            while (true) {
                const std::size_t end = text_.find('\n', begin);
                const std::string_view line = text_.substr(begin, end == std::string_view::npos ? end : end - begin);
                const float line_width = anchor_.x != 0.f ? measure_text(*font_, line, size_).x : 0.f;
                Vec2 pen{position_.x - std::round(line_width * anchor_.x), y};
                scan_text(
                    *font_, line,
                    [&](const Glyph &glyph) noexcept {
                        f({pen + glyph.bearing * scale, glyph.atlas.size() * scale, &glyph});
                        pen.x += glyph.advance * scale;
                    },
                    [] noexcept {}
                );
                if (end == std::string_view::npos) {
                    return;
                }
                y += size_;
                begin = end + 1;
            }
        }

    private:
        const FontData *font_;
        std::string_view text_;
        float size_;
        Vec2 position_;
        Vec2 anchor_;
        mutable Vec2 extent_{};
        mutable bool measured_{false};
    };
}
