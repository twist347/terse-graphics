#pragma once

#include "tgx/error.h"
#include "tgx/handle.h"
#include "tgx/size.h"

namespace tgx {
    class Image;

    namespace detail {
        auto delete_texture(GlId id) noexcept -> void;
    }

    enum class TextureAccess {
        // Contents are fixed at creation.
        immutable,
        // Contents can be rewritten with Texture::update.
        dynamic,
    };

    // How a texel is picked when the texture is drawn larger or smaller than
    // it is.
    enum class TextureFilter {
        // The closest texel: sharp squares, for pixel art.
        nearest,
        // A blend of the four closest texels: smooth.
        linear,
    };

    // What texture coordinates outside [0, 1] read, on both axes.
    enum class TextureWrap {
        // The edge texels, stretched.
        clamp,
        // The texture again, tiled.
        repeat,
        // The texture again, every other copy flipped.
        mirror,
    };

    struct TextureParams {
        TextureFilter filter{TextureFilter::linear};
        TextureWrap wrap{TextureWrap::clamp};
        // Halved copies down to 1x1, built at creation and after every update,
        // so a texture drawn much smaller than it is does not shimmer. A third
        // more memory; 2D drawing near natural size does without.
        bool mipmaps{false};
        TextureAccess access{TextureAccess::immutable};
    };

    // A 2D RGBA8 texture on the GPU, of a size fixed at creation. Texture
    // coordinates (0, 0) are the top-left pixel of the image it was made from,
    // (1, 1) the bottom-right.
    //
    // Unlike the gl:: resources it has nothing GL-specific to configure, so it
    // serves both the Canvas and Device::draw; id() is the way out to raw GL.
    //
    // Lives inside the Device: created after it, destroyed before it. Sprites
    // of it still waiting in the Device's batch are drawn first when it is
    // updated or goes.
    class Texture {
    public:
        // Filled from the image, which must not be empty. Fails with
        // Error::unsupported when a side exceeds what the driver allows.
        [[nodiscard]] static auto create(
            const Image &image,
            const TextureParams &params = {}
        ) noexcept -> Result<Texture>;

        // Uninitialised, to be filled with update(); only useful as dynamic.
        [[nodiscard]] static auto create(
            Size size,
            const TextureParams &params
        ) noexcept -> Result<Texture>;

        // Overwrites the pixels the image covers when its top-left pixel is put
        // at (x, y); only for dynamic textures, and the image must fit.
        auto update(int x, int y, const Image &image) noexcept -> void;

        [[nodiscard]] auto id() const noexcept -> GlId;

        [[nodiscard]] auto size() const noexcept -> Size;

        [[nodiscard]] auto params() const noexcept -> const TextureParams &;

    private:
        Texture(GlId id, Size size, const TextureParams &params) noexcept
            : m_handle{id}, m_size{size}, m_params{params} {
        }

        detail::Handle<detail::delete_texture> m_handle;
        Size m_size{};
        TextureParams m_params{};
    };
}
