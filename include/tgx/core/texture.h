#pragma once

#include "tgx/core/error.h"
#include "tgx/core/handle.h"
#include "tgx/core/math.h"

#include <cstdint>
#include <filesystem>

namespace tgx {
    class Image;

    namespace detail {
        auto delete_texture(GlId id) noexcept -> void;
    }

    enum class TextureAccess : std::int32_t {
        // Contents are fixed at creation.
        immutable,
        // Contents can be rewritten with Texture::update.
        dynamic,
    };

    // How a texel is picked when the texture is drawn larger or smaller than
    // it is.
    enum class TextureFilter : std::int32_t {
        // The closest texel: sharp squares, for pixel art.
        nearest,
        // A blend of the four closest texels: smooth.
        linear,
    };

    // What texture coordinates outside [0, 1] read, on both axes.
    enum class TextureWrap : std::int32_t {
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
    // (1, 1) the bottom-right; a render target's is the other way up
    // (bottom_up()).
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
        // Error::unsupported when a side exceeds what the driver allows,
        // Error::out_of_memory when the GPU has no room for it, Error::platform
        // when the driver fails otherwise.
        [[nodiscard]] static auto create(
            const Image &image,
            const TextureParams &params = {}
        ) noexcept -> Result<Texture>;

        // Image::load and create() in one, failing as either does: Error::io
        // or Error::decode for the file, then as create() for the texture.
        //
        //     auto player = tgx::Texture::load("player.png", {.filter = tgx::TextureFilter::nearest});
        [[nodiscard]] static auto load(
            const std::filesystem::path &path,
            const TextureParams &params = {}
        ) -> Result<Texture>;

        // Uninitialized, to be filled with update(): params.access must be
        // dynamic (asserted). Fails as the create() above.
        [[nodiscard]] static auto create(
            Size size,
            const TextureParams &params
        ) noexcept -> Result<Texture>;

        Texture(const Texture &) = delete;
        auto operator=(const Texture &) -> Texture & = delete;

        Texture(Texture &&) noexcept = default;
        auto operator=(Texture &&) noexcept -> Texture & = default;

        // Overwrites the pixels the image covers when its top-left pixel is put
        // at (x, y); only for dynamic textures, and the image must fit.
        auto update(int x, int y, const Image &image) noexcept -> void;

        [[nodiscard]] auto id() const noexcept -> GlId { return m_handle.get(); }

        [[nodiscard]] auto size() const noexcept -> Size { return m_size; }

        [[nodiscard]] auto params() const noexcept -> const TextureParams & { return m_params; }

        // Whether its rows run bottom to top, as GL draws: true only for a
        // RenderTarget's. Sprites of it come out the right way up anyway; a
        // shader of your own reads v = 0 as its bottom.
        [[nodiscard]] auto bottom_up() const noexcept -> bool { return m_bottom_up; }

    private:
        friend class RenderTarget;

        Texture(GlId id, Size size, const TextureParams &params, bool bottom_up) noexcept
            : m_handle{id}, m_size{size}, m_params{params}, m_bottom_up{bottom_up} {
        }

        // Storage for a RenderTarget to draw into.
        [[nodiscard]] static auto create_for_target(
            Size size,
            const TextureParams &params
        ) noexcept -> Result<Texture>;

        detail::Handle<detail::delete_texture> m_handle;
        Size m_size{};
        TextureParams m_params{};
        bool m_bottom_up{false};
    };
}
