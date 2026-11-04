#pragma once

#include "tgx/core/math.hpp"

#include <cstddef>
#include <span>

namespace tgx {
    class Image;
}

namespace tgx::detail {
    // CPU metrics for one codepoint. Glyphs are sorted by codepoint.
    struct Glyph {
        char32_t codepoint{};
        Rect atlas{};
        Vec2 bearing{};
        float advance{};
        bool blank{false};
    };

    struct FontData {
        Size atlas_size{};
        float line_height{};
        std::span<const Glyph> glyphs{};
        std::size_t fallback{};

        [[nodiscard]] auto glyph(char32_t codepoint) const noexcept -> const Glyph &;
    };

    [[nodiscard]] auto default_font_data() noexcept -> const FontData &;

    // The default glyphs, white on transparent, and the white block for shapes.
    [[nodiscard]] auto make_default_font_atlas() -> Image;
}
