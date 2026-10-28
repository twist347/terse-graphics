#pragma once

#include "tgx/core/color.hpp"

#include <glad/gl.h>

// Pixels between tgx and GL: a Color's four bytes, red first. Rows are always
// 4-byte aligned, GL's default for packing and unpacking.
namespace tgx::detail {
    static_assert(sizeof(Color) == 4, "a Color is one RGBA8 pixel");

    // In textures.
    inline constexpr GLenum pixel_storage = GL_RGBA8;

    // For uploads and read backs.
    inline constexpr GLenum pixel_format = GL_RGBA;
    inline constexpr GLenum pixel_type = GL_UNSIGNED_BYTE;

    // Largest width or height of a texture or render target. A bigger one
    // fails with Error::unsupported.
    [[nodiscard]] inline auto max_texture_size() noexcept -> int {
        GLint size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
        return size;
    }
}
