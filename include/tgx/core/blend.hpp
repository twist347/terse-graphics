#pragma once

#include <cstdint>

namespace tgx {
    // How a draw's colors combine with the framebuffer's. Colors are straight
    // (not premultiplied) unless the mode says otherwise.
    enum class Blend : std::int32_t {
        // Overwrites the framebuffer; alpha has no effect.
        none,
        // src * a + dst * (1 - a): plain transparency.
        alpha,
        // src + dst * (1 - a), for premultiplied colors.
        premultiplied,
        // src * a + dst: adds light, for glows and particles.
        additive,
        // src * dst, fading towards dst as alpha drops: for darkening and
        // tinting. Needs premultiplied colors (Color::premultiplied()); a
        // straight transparent color comes out lighter than dst.
        multiply,
    };
}
