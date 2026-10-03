#pragma once

#include <cstdint>

namespace tgx {
    // How a draw's colors combine with what the framebuffer already holds.
    // Colors are straight (not premultiplied) unless the mode says otherwise.
    enum class Blend : std::int32_t {
        // Overwrites; alpha has no effect.
        none,
        // src * a + dst * (1 - a): ordinary transparency.
        alpha,
        // src + dst * (1 - a), for colors already multiplied by their alpha.
        premultiplied,
        // src * a + dst: light adding up, as in glows and particles.
        additive,
        // src * dst, faded towards dst as alpha drops: darkening and tinting.
        // Like premultiplied, it wants colors already multiplied by their alpha
        // (Color::premultiplied()); a straight see-through color would come out
        // lighter than dst. Opaque colors are the same either way.
        multiply,
    };
}
