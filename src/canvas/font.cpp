#include "canvas/font.hpp"

#include "tgx/core/image.hpp"

#include "base/default_font.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace {
    [[nodiscard]] consteval auto make_default_glyphs() {
        namespace baked = tgx::detail::default_font;
        std::array<tgx::detail::Glyph, baked::glyphs.size()> result{};
        for (std::size_t i = 0; i < result.size(); ++i) {
            const auto source = baked::glyphs[i];
            const auto codepoint = static_cast<char32_t>(baked::first + static_cast<char>(i));
            result[i] = {
                .codepoint = codepoint,
                .atlas = {
                    static_cast<float>(source.x), 0.f,
                    static_cast<float>(source.width), static_cast<float>(baked::line_height),
                },
                .advance = static_cast<float>(source.width),
                .blank = codepoint == U' ',
            };
        }
        return result;
    }

    constexpr auto default_glyphs = make_default_glyphs();
    // Glyphs are one row filling the atlas height, so v goes from 0 to 1.
    static_assert(tgx::detail::default_font::atlas_height == tgx::detail::default_font::line_height);
}

namespace tgx::detail {
    auto default_solid_uv() noexcept -> Vec2 {
        namespace baked = default_font;
        return {
            (static_cast<float>(baked::white_x) + baked::white_size / 2.f) / baked::atlas_width,
            (baked::white_size / 2.f) / baked::atlas_height,
        };
    }

    auto FontData::glyph(char32_t codepoint) const noexcept -> const Glyph & {
        if (!glyphs.empty()) {
            const char32_t first = glyphs.front().codepoint;
            const char32_t last = glyphs.back().codepoint;
            if (last - first + 1 == glyphs.size() && codepoint >= first && codepoint <= last) {
                return glyphs[codepoint - first];
            }
        }
        const auto found = std::ranges::lower_bound(glyphs, codepoint, {}, &Glyph::codepoint);
        if (found != glyphs.end() && found->codepoint == codepoint) {
            return *found;
        }
        return glyphs[fallback];
    }

    auto default_font_data() noexcept -> const FontData & {
        namespace baked = default_font;
        static constexpr FontData data{
            .atlas_size = {baked::atlas_width, baked::atlas_height},
            .line_height = static_cast<float>(baked::line_height),
            .glyphs = default_glyphs,
            .fallback = static_cast<std::size_t>('?' - baked::first),
        };
        return data;
    }

    auto make_default_font_atlas() -> Image {
        namespace baked = default_font;
        auto image = Image::create({baked::atlas_width, baked::atlas_height});
        for (int y = 0; y < baked::atlas_height; ++y) {
            for (int x = 0; x < baked::atlas_width; ++x) {
                const auto byte = baked::bits[static_cast<std::size_t>(y * baked::row_bytes + x / 8)];
                if ((byte & (0x80 >> (x % 8))) != 0) {
                    image[x, y] = colors::white;
                }
            }
        }
        return image;
    }
}
