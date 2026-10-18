#pragma once

#include "tgx/core/error.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/math.hpp"

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
        // Rewritable with Texture::update.
        dynamic,
    };

    // Sampling when drawn larger or smaller than its size.
    enum class TextureFilter : std::int32_t {
        // Sharp squares, for pixel art.
        nearest,
        // Smooth: a blend of the four nearest texels.
        linear,
    };

    // What coordinates outside [0, 1] read, on both axes.
    enum class TextureWrap : std::int32_t {
        // The edge texels.
        clamp,
        // The texture tiled.
        repeat,
        // Tiled, every other copy mirrored.
        mirror,
    };

    struct TextureParams {
        TextureFilter filter{TextureFilter::linear};
        TextureWrap wrap{TextureWrap::clamp};
        // Halved copies down to 1x1, rebuilt after every update, so a texture
        // drawn much smaller does not shimmer. A third more memory; 2D drawing
        // near natural size does not need them.
        bool mipmaps{false};
        TextureAccess access{TextureAccess::immutable};
    };

    // A 2D RGBA8 texture on the GPU, fixed size. (0, 0) is the image's
    // top-left pixel, (1, 1) the bottom-right; a render target's texture is
    // upside down (bottom_up()). Used by both the Canvas and Device::draw.
    //
    // Create it after the Device and destroy it before the Device. Waiting
    // sprites of it are drawn before it is updated or destroyed.
    class Texture {
    public:
        // The image must not be empty. Fails with Error::unsupported over the
        // driver's size limit, Error::out_of_memory, or Error::platform.
        [[nodiscard]] static auto create(
            const Image &image,
            const TextureParams &params = {}
        ) noexcept -> Result<Texture>;

        // Image::load and create() in one call, and fails like either.
        //
        //     auto player = tgx::Texture::load("player.png", {.filter = tgx::TextureFilter::nearest});
        [[nodiscard]] static auto load(
            const std::filesystem::path &path,
            const TextureParams &params = {}
        ) -> Result<Texture>;

        // Uninitialized, to fill with update(). params.access must be dynamic
        // (asserted). Fails like the create() above.
        [[nodiscard]] static auto create(
            Size size,
            const TextureParams &params
        ) noexcept -> Result<Texture>;

        Texture(const Texture &) = delete;
        auto operator=(const Texture &) -> Texture & = delete;

        Texture(Texture &&) noexcept = default;
        auto operator=(Texture &&) noexcept -> Texture & = default;

        // The image's top-left goes at (x, y). Dynamic textures only; the
        // image must fit.
        auto update(int x, int y, const Image &image) noexcept -> void;

        [[nodiscard]] auto id() const noexcept -> GlId { return handle_.get(); }

        [[nodiscard]] auto size() const noexcept -> Size { return size_; }

        [[nodiscard]] auto params() const noexcept -> const TextureParams & { return params_; }

        // Rows go bottom to top: only a RenderTarget's texture. Sprites are
        // still drawn upright; in your own shader v = 0 is the bottom.
        [[nodiscard]] auto bottom_up() const noexcept -> bool { return bottom_up_; }

    private:
        friend class RenderTarget;

        Texture(GlId id, Size size, const TextureParams &params, bool bottom_up) noexcept
            : handle_{id}, size_{size}, params_{params}, bottom_up_{bottom_up} {
        }

        // Storage for a RenderTarget to draw into.
        [[nodiscard]] static auto create_for_target(
            Size size,
            const TextureParams &params
        ) noexcept -> Result<Texture>;

        detail::Handle<detail::delete_texture> handle_{};
        Size size_{};
        TextureParams params_{};
        bool bottom_up_{false};
    };
}
