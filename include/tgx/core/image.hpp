#pragma once

#include "tgx/core/assert.hpp"
#include "tgx/core/color.hpp"
#include "tgx/core/error.hpp"
#include "tgx/core/math.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <utility>
#include <vector>

namespace tgx {
    enum class ImageFilter : std::int32_t {
        nearest,
        linear,
    };

    // RGBA8 pixels in memory, rows top to bottom like image files. Empty by
    // default.
    class Image {
    public:
        Image() noexcept = default;

        // Every pixel is fill.
        [[nodiscard]] static auto create(Size size, Color fill = colors::transparent) -> Image;

        // Exactly width * height pixels, copied.
        [[nodiscard]] static auto from_pixels(Size size, std::span<const Color> pixels) -> Image;

        // PNG, JPEG, BMP, TGA or GIF (first frame). Fails with Error::io when
        // the file cannot be read, Error::decode when it is not such an image.
        [[nodiscard]] static auto load(const std::filesystem::path &path) -> Result<Image>;

        // Like load(), from a file's bytes in memory, e.g. an embedded asset.
        // Fails with Error::decode.
        [[nodiscard]] static auto decode(std::span<const std::byte> encoded) -> Result<Image>;

        // As PNG, creating or replacing the file; a failed write leaves the
        // old file. Must not be empty. Fails with Error::io, with
        // Error::out_of_memory when encoding fails, and with
        // Error::unsupported when the RGBA data approaches 2 GiB (roughly
        // 537 million pixels; the exact limit also depends on the height).
        [[nodiscard]] auto save(const std::filesystem::path &path) const -> Result<void>;

        // A copy of a whole-pixel rectangle. (x, y) and size must fit inside
        // the image (asserted). An empty size gives an empty image.
        [[nodiscard]] auto crop(int x, int y, Size size) const -> Image;

        // A resized copy. Pixel centers are sampled; linear interpolates all
        // four straight RGBA channels. An empty source gives transparent
        // pixels; an empty destination is empty.
        [[nodiscard]] auto resize(Size size, ImageFilter filter = ImageFilter::linear) const -> Image;

        // In place, mirrored left-right or top-bottom.
        auto flip_horizontal() noexcept -> void;
        auto flip_vertical() noexcept -> void;

        [[nodiscard]] auto size() const noexcept -> Size { return size_; }

        [[nodiscard]] auto empty() const noexcept -> bool { return size_.empty(); }

        // Row by row. Read-only for a const Image.
        [[nodiscard]] auto pixels(this auto &self) noexcept -> auto { return std::span{self.pixels_}; }

        // (0, 0) is the top-left pixel.
        [[nodiscard]] auto operator[](int x, int y) noexcept -> Color & { return pixels_[index(x, y)]; }
        [[nodiscard]] auto operator[](int x, int y) const noexcept -> Color { return pixels_[index(x, y)]; }

    private:
        Image(Size size, std::vector<Color> pixels) noexcept : size_{size}, pixels_{std::move(pixels)} {
        }

        [[nodiscard]] auto index(int x, int y) const noexcept -> std::size_t {
            TGX_ASSERT_MSG(
                x >= 0 && x < size_.width && y >= 0 && y < size_.height,
                "pixel ({}, {}) is outside a {}x{} image",
                x, y, size_.width, size_.height
            );
            return static_cast<std::size_t>(y) * static_cast<std::size_t>(size_.width)
                   + static_cast<std::size_t>(x);
        }

        Size size_{};
        std::vector<Color> pixels_{};
    };
}
