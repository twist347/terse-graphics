#include "tgx/core/texture.hpp"

#include "tgx/core/assert.hpp"
#include "tgx/core/image.hpp"

#include "core/context.hpp"
#include "core/gl_error.hpp"
#include "core/gl_pixels.hpp"

#include <glad/gl.h>

#include <cstdint>

namespace {
    // The slot for edits. No binding target is free of draws, unlike for
    // buffers, so it goes through the Device's cache and the next draw
    // rebinds it.
    constexpr std::uint32_t edit_slot = 0;

    [[nodiscard]] constexpr auto to_gl(tgx::TextureWrap wrap) noexcept -> GLint {
        using enum tgx::TextureWrap;
        switch (wrap) {
            case clamp: return GL_CLAMP_TO_EDGE;
            case repeat: return GL_REPEAT;
            case mirror: return GL_MIRRORED_REPEAT;
        }
        return GL_CLAMP_TO_EDGE;
    }

    // Always set: the default min filter needs mipmaps, so a texture without
    // them would read as black.
    [[nodiscard]] constexpr auto min_filter(const tgx::TextureParams &params) noexcept -> GLint {
        const bool linear = params.filter == tgx::TextureFilter::linear;
        if (!params.mipmaps) {
            return linear ? GL_LINEAR : GL_NEAREST;
        }
        return linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
    }

    [[nodiscard]] constexpr auto mag_filter(const tgx::TextureParams &params) noexcept -> GLint {
        return params.filter == tgx::TextureFilter::linear ? GL_LINEAR : GL_NEAREST;
    }

    [[nodiscard]] auto make(
        tgx::Size size,
        const void *pixels,
        const tgx::TextureParams &params
    ) noexcept -> tgx::Result<GLuint> {
        TGX_ASSERT_MSG(!size.empty(), "texture of size {}x{}", size.width, size.height);

        // A driver limit, not a caller error: it differs between GPUs.
        if (const int max_size = tgx::detail::max_texture_size(); size.width > max_size || size.height > max_size) {
            return std::unexpected{tgx::Error::unsupported};
        }

        tgx::detail::drain_gl_errors();

        GLuint id = 0;
        glGenTextures(1, &id);
        tgx::detail::context().bind_texture(edit_slot, id);

        glTexImage2D(
            GL_TEXTURE_2D, 0, tgx::detail::pixel_storage,
            size.width, size.height, 0,
            tgx::detail::pixel_format, tgx::detail::pixel_type, pixels
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter(params));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter(params));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, to_gl(params.wrap));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, to_gl(params.wrap));

        // No data yet, but the levels must exist for the texture to be
        // complete. update() rebuilds them.
        if (params.mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        // One check for all of the above; the levels need memory too.
        return tgx::detail::checked_allocation<tgx::detail::delete_texture>(id);
    }
}

namespace tgx {
    auto detail::delete_texture(GlId id) noexcept -> void {
        // Draws the waiting sprites while the texture exists.
        detail::flush_texture_use(id);
        detail::context().forget_texture(id);
        glDeleteTextures(1, &id);
    }

    auto Texture::create(
        const Image &image,
        const TextureParams &params
    ) noexcept -> Result<Texture> {
        return make(image.size(), image.pixels().data(), params).transform([&](GLuint id) {
            return Texture{id, image.size(), params, false};
        });
    }

    auto Texture::load(const std::filesystem::path &path, const TextureParams &params) -> Result<Texture> {
        return Image::load(path).and_then([&](const Image &image) {
            return create(image, params);
        });
    }

    auto Texture::create(
        Size size,
        const TextureParams &params
    ) noexcept -> Result<Texture> {
        TextureParams dynamic = params;
        dynamic.access = TextureAccess::dynamic;
        return make(size, nullptr, dynamic).transform([&](GLuint id) {
            return Texture{id, size, dynamic, false};
        });
    }

    auto Texture::update(int x, int y, const Image &image) noexcept -> void {
        TGX_ASSERT_MSG(params_.access == TextureAccess::dynamic, "updating an immutable texture");

        const Size size = image.size();
        TGX_ASSERT_MSG(
            x >= 0 && y >= 0 && size.width <= size_.width - x && size.height <= size_.height - y,
            "a {}x{} image at ({}, {}) overruns a {}x{} texture",
            size.width, size.height, x, y, size_.width, size_.height
        );

        if (image.empty()) {
            return;
        }

        // Waiting sprites use the old pixels.
        detail::flush_texture_use(handle_.get());
        detail::context().bind_texture(edit_slot, handle_.get());
        glTexSubImage2D(
            GL_TEXTURE_2D, 0,
            x, y, size.width, size.height,
            detail::pixel_format, detail::pixel_type, image.pixels().data()
        );

        if (params_.mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    auto Texture::create_for_target(
        Size size,
        const TextureParams &params
    ) noexcept -> Result<Texture> {
        return make(size, nullptr, params).transform([&](GLuint id) {
            return Texture{id, size, params, true};
        });
    }
}
