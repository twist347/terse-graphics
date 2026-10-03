#include "tgx/image.h"

#include "tgx/assert.h"

#include <stb/stb_image.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

namespace {
    static_assert(sizeof(tgx::Color) == 4, "stb's RGBA pixels are copied straight into Colors");

    [[nodiscard]] auto pixel_count(tgx::Size size) noexcept -> std::size_t {
        TGX_ASSERT_MSG(size.width >= 0 && size.height >= 0, "image of negative size {}x{}", size.width, size.height);

        return static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height);
    }

    struct StbFree {
        auto operator()(stbi_uc *pixels) const noexcept -> void {
            stbi_image_free(pixels);
        }
    };

    // The whole file, or nothing if it cannot be opened or read through.
    [[nodiscard]] auto read_file(const std::filesystem::path &path) -> tgx::Result<std::vector<std::byte>> {
        // A directory opens fine on some systems and reports a size that is
        // anything but its contents (LLONG_MAX on ext4).
        std::error_code err;
        if (!std::filesystem::is_regular_file(path, err)) {
            return std::unexpected{tgx::Error::io};
        }

        std::ifstream file{path, std::ios::binary | std::ios::ate};
        if (!file) {
            return std::unexpected{tgx::Error::io};
        }

        // Opened at the end, so the position is the size; -1 for a stream that
        // cannot tell.
        const std::streamoff size = file.tellg();
        if (size < 0) {
            return std::unexpected{tgx::Error::io};
        }

        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))) {
            return std::unexpected{tgx::Error::io};
        }
        return bytes;
    }
}

namespace tgx {
    auto Image::create(Size size, Color fill) -> Image {
        return Image{size, std::vector<Color>(pixel_count(size), fill)};
    }

    auto Image::from_pixels(Size size, std::span<const Color> pixels) -> Image {
        TGX_ASSERT_MSG(
            pixels.size() == pixel_count(size),
            "{} pixels for a {}x{} image",
            pixels.size(), size.width, size.height
        );

        return Image{size, std::vector<Color>(pixels.begin(), pixels.end())};
    }

    auto Image::load(const std::filesystem::path &path) -> Result<Image> {
        return read_file(path).and_then([](const std::vector<std::byte> &bytes) {
            return decode(bytes);
        });
    }

    auto Image::decode(std::span<const std::byte> encoded) -> Result<Image> {
        // stb takes the length as an int. No image file of 2 GiB or more is
        // within its limits anyway.
        if (!std::in_range<int>(encoded.size())) {
            return std::unexpected{Error::decode};
        }

        // Asked for 4 channels, stb converts whatever the file holds (gray,
        // RGB, 16 bits per channel) to RGBA8, rows top to bottom.
        int width = 0;
        int height = 0;
        int channels_in_file = 0;
        const std::unique_ptr<stbi_uc, StbFree> pixels{stbi_load_from_memory(
            reinterpret_cast<const stbi_uc *>(encoded.data()),
            static_cast<int>(encoded.size()),
            &width, &height, &channels_in_file, 4
        )};
        if (pixels == nullptr) {
            return std::unexpected{Error::decode};
        }

        const Size size{width, height};
        std::vector<Color> colors(pixel_count(size));
        std::memcpy(colors.data(), pixels.get(), colors.size() * sizeof(Color));
        return Image{size, std::move(colors)};
    }
}
