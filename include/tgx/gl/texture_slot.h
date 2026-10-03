#pragma once

#include <cstddef>
#include <cstdint>

namespace tgx::gl {
    // Texture slots a draw can fill (DrawParams::textures). GL 3.3 guarantees
    // 16 for the fragment stage; 2D drawing needs a few.
    inline constexpr std::size_t max_texture_slots = 8;

    // The value of a sampler2D uniform: which slot of the draw it reads. Set
    // once after creating the shader; GL starts every sampler at slot 0.
    //
    //     shader.set(shader.uniform<gl::TextureSlot>("u_texture"), {0});
    struct TextureSlot {
        std::uint32_t index{0};
    };
}
