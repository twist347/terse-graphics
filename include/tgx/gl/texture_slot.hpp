#pragma once

#include <cstddef>
#include <cstdint>

namespace tgx::gl {
    // Slots per draw (DrawParams::textures). GL 3.3 guarantees 16 for the
    // fragment stage; 2D drawing needs a few.
    inline constexpr std::size_t max_texture_slots = 8;

    // The value of a sampler2D uniform: the draw's slot it reads from. Set it
    // once after creating the shader; GL starts every sampler at 0.
    //
    //     shader.set(shader.uniform<gl::TextureSlot>("u_texture"), {0});
    struct TextureSlot {
        std::uint32_t index{0};
    };
}
