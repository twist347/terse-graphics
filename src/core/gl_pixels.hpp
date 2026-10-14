#pragma once

#include "tgx/core/color.hpp"

#include <glad/gl.h>

// How pixels travel between tgx and GL: a Color's four bytes, red first. Rows
// of 4-byte pixels are always 4-byte aligned, GL's default for packing and
// unpacking alike, so neither needs setting.
namespace tgx::detail {
    static_assert(sizeof(Color) == 4, "a Color is one RGBA8 pixel");

    // As textures keep them.
    inline constexpr GLenum pixel_storage = GL_RGBA8;

    // As they are uploaded and read back.
    inline constexpr GLenum pixel_format = GL_RGBA;
    inline constexpr GLenum pixel_type = GL_UNSIGNED_BYTE;

    // The largest width or height a texture or render target may have on
    // this GPU: past it, creating one fails as unsupported.
    [[nodiscard]] inline auto max_texture_size() noexcept -> int {
        GLint size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
        return size;
    }
}
