#pragma once

#include "tgx/core/assert.h"
#include "tgx/core/color.h"
#include "tgx/core/error.h"
#include "tgx/core/math.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <utility>
#include <vector>

namespace tgx {
    // RGBA8 pixels in memory, rows top to bottom: the order image files and
    // editors use. A default-constructed Image is empty.
    class Image {
    public:
        Image() noexcept = default;

        // Every pixel set to fill.
        [[nodiscard]] static auto create(Size size, Color fill = colors::transparent) -> Image;

        // A copy of the pixels, which must number width * height.
        [[nodiscard]] static auto from_pixels(Size size, std::span<const Color> pixels) -> Image;

        // Reads a PNG, JPEG, BMP, TGA or GIF (its first frame) file. Fails with
        // Error::io when the file cannot be read, Error::decode when it is not
        // an image in one of those formats.
        [[nodiscard]] static auto load(const std::filesystem::path &path) -> Result<Image>;

        // The same from the bytes of such a file already in memory, e.g. an
        // embedded asset. Fails with Error::decode.
        [[nodiscard]] static auto decode(std::span<const std::byte> encoded) -> Result<Image>;

        // Writes it as a PNG file, made or replaced. Fails with Error::io when
        // the file cannot be written, Error::out_of_memory when it cannot be
        // encoded. The image must not be empty.
        [[nodiscard]] auto save(const std::filesystem::path &path) const -> Result<void>;

        [[nodiscard]] auto size() const noexcept -> Size { return m_size; }

        [[nodiscard]] auto empty() const noexcept -> bool { return m_size.empty(); }

        // Row by row, width pixels each.
        [[nodiscard]] auto pixels() noexcept -> std::span<Color> { return m_pixels; }
        [[nodiscard]] auto pixels() const noexcept -> std::span<const Color> { return m_pixels; }

        // (0, 0) is the top-left pixel.
        [[nodiscard]] auto at(int x, int y) noexcept -> Color & { return m_pixels[index(x, y)]; }
        [[nodiscard]] auto at(int x, int y) const noexcept -> Color { return m_pixels[index(x, y)]; }

    private:
        Image(Size size, std::vector<Color> pixels) noexcept : m_size{size}, m_pixels{std::move(pixels)} {
        }

        [[nodiscard]] auto index(int x, int y) const noexcept -> std::size_t {
            TGX_ASSERT_MSG(
                x >= 0 && x < m_size.width && y >= 0 && y < m_size.height,
                "pixel ({}, {}) is outside a {}x{} image",
                x, y, m_size.width, m_size.height
            );
            return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_size.width)
                   + static_cast<std::size_t>(x);
        }

        Size m_size{};
        std::vector<Color> m_pixels;
    };
}
