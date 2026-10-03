#include "tgx/texture.h"

#include "tgx/assert.h"
#include "tgx/image.h"

#include "context.h"

#include <glad/gl.h>

#include <cstdint>

namespace {
    // Textures are bound here to be edited. Unlike buffers there is no target a
    // draw does not read, so the binding goes through the Device's cache: the
    // next draw that wants another texture in this slot rebinds it.
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

    // Both filters are always set: the default minification filter wants
    // mipmaps, and a texture without them would then read as black.
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

        // Too large is up to the driver, not the caller: the limit differs from
        // one GPU to the next.
        GLint max_size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
        if (size.width > max_size || size.height > max_size) {
            return std::unexpected{tgx::Error::unsupported};
        }

        // See make() in buffer.cpp: only this allocation's errors count.
        for (int i = 0; i < 16 && glGetError() != GL_NO_ERROR; ++i) {
        }

        GLuint id = 0;
        glGenTextures(1, &id);
        tgx::gl::detail::context().bind_texture(edit_slot, id);

        // Rows of 4-byte pixels are always 4-byte aligned, GL's default unpack
        // alignment, so it needs no setting.
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8,
            size.width, size.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, pixels
        );

        if (const GLenum err = glGetError(); err != GL_NO_ERROR) {
            tgx::detail::delete_texture(id);
            return std::unexpected{err == GL_OUT_OF_MEMORY ? tgx::Error::out_of_mem : tgx::Error::platform};
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter(params));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter(params));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, to_gl(params.wrap));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, to_gl(params.wrap));

        // Without data there is nothing to halve yet, but the levels must exist
        // for the texture to be complete; update() rebuilds them.
        if (params.mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        return id;
    }
}

namespace tgx {
    auto detail::delete_texture(gl::GlId id) noexcept -> void {
        // Sprites added before keep the texture they were added with.
        gl::detail::flush_texture_use(id);
        gl::detail::context().forget_texture(id);
        glDeleteTextures(1, &id);
    }

    auto Texture::create(
        const Image &image,
        const TextureParams &params
    ) noexcept -> Result<Texture> {
        return make(image.size(), image.pixels().data(), params).transform([&](GLuint id) {
            return Texture{id, image.size(), params};
        });
    }

    auto Texture::create(
        Size size,
        const TextureParams &params
    ) noexcept -> Result<Texture> {
        TGX_ASSERT_MSG(params.access == TextureAccess::dynamic, "an immutable texture without data can never be filled");

        return make(size, nullptr, params).transform([&](GLuint id) {
            return Texture{id, size, params};
        });
    }

    auto Texture::update(int x, int y, const Image &image) noexcept -> void {
        TGX_ASSERT_MSG(m_params.access == TextureAccess::dynamic, "updating an immutable texture");

        const Size size = image.size();
        TGX_ASSERT_MSG(
            x >= 0 && y >= 0 && size.width <= m_size.width - x && size.height <= m_size.height - y,
            "a {}x{} image at ({}, {}) overruns a {}x{} texture",
            size.width, size.height, x, y, m_size.width, m_size.height
        );

        if (image.empty()) {
            return;
        }

        // Sprites added before keep the pixels they were added with.
        gl::detail::flush_texture_use(m_handle.get());
        gl::detail::context().bind_texture(edit_slot, m_handle.get());
        glTexSubImage2D(
            GL_TEXTURE_2D, 0,
            x, y, size.width, size.height,
            GL_RGBA, GL_UNSIGNED_BYTE, image.pixels().data()
        );

        if (m_params.mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    auto Texture::id() const noexcept -> gl::GlId {
        return m_handle.get();
    }

    auto Texture::size() const noexcept -> Size {
        return m_size;
    }

    auto Texture::params() const noexcept -> const TextureParams & {
        return m_params;
    }
}
