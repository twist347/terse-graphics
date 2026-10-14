#pragma once

#include "tgx/core/assert.hpp"
#include "tgx/core/color.hpp"
#include "tgx/core/error.hpp"
#include "tgx/core/math.hpp"

#include <cstddef>
#include <filesystem>
#include <span>
#include <utility>
#include <vector>

namespace tgx {
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
        // Error::unsupported at 2 GiB of pixels or more.
        [[nodiscard]] auto save(const std::filesystem::path &path) const -> Result<void>;

        [[nodiscard]] auto size() const noexcept -> Size { return m_size; }

        [[nodiscard]] auto empty() const noexcept -> bool { return m_size.empty(); }

        // Row by row. Read-only for a const Image.
        [[nodiscard]] auto pixels(this auto &self) noexcept -> auto { return std::span{self.m_pixels}; }

        // (0, 0) is the top-left pixel.
        [[nodiscard]] auto operator[](int x, int y) noexcept -> Color & { return m_pixels[index(x, y)]; }
        [[nodiscard]] auto operator[](int x, int y) const noexcept -> Color { return m_pixels[index(x, y)]; }

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
