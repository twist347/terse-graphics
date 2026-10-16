#include "tgx/core/image.hpp"

#include "tgx/core/assert.hpp"

#include "core/file.hpp"

// Compiled here with internal linkage, so an app may link its own stb.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace {
    // stb's RGBA pixels have Color's layout, so they are copied directly.
    constexpr int channels = 4;
    static_assert(sizeof(tgx::Color) == channels);

    [[nodiscard]] auto pixel_count(tgx::Size size) noexcept -> std::size_t {
        TGX_ASSERT_MSG(size.width >= 0 && size.height >= 0, "image of negative size {}x{}", size.width, size.height);

        return static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height);
    }

    struct StbFree {
        auto operator()(stbi_uc *pixels) const noexcept -> void {
            stbi_image_free(pixels);
        }
    };
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
        return detail::read_file(path).and_then(decode);
    }

    auto Image::decode(std::span<const std::byte> encoded) -> Result<Image> {
        // stb takes the length as an int; 2 GiB is beyond its limits anyway.
        if (!std::in_range<int>(encoded.size())) {
            return std::unexpected{Error::decode};
        }

        // stb converts any format (gray, RGB, 16-bit) to RGBA8, top to bottom.
        int width = 0;
        int height = 0;
        int channels_in_file = 0;
        const std::unique_ptr<stbi_uc, StbFree> pixels{
            stbi_load_from_memory(
                reinterpret_cast<const stbi_uc *>(encoded.data()),
                static_cast<int>(encoded.size()),
                &width, &height, &channels_in_file, channels
            )
        };
        if (!pixels) {
            return std::unexpected{Error::decode};
        }

        const Size size{width, height};
        std::vector<Color> colors(pixel_count(size));
        std::memcpy(colors.data(), pixels.get(), colors.size() * sizeof(Color));
        return Image{size, std::move(colors)};
    }

    auto Image::save(const std::filesystem::path &path) const -> Result<void> {
        TGX_ASSERT_MSG(!empty(), "saving an empty image");

        // stb counts the bytes of a row and of the filtered image in an int
        // and writes garbage beyond it. No PNG reader accepts that size anyway.
        constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<int>::max());
        if (pixel_count(size_) >= limit / sizeof(Color) - static_cast<std::size_t>(size_.height)) {
            return std::unexpected{Error::unsupported};
        }

        // Encodes into memory, so the path is opened the same way as when
        // reading.
        struct Encoded {
            std::vector<std::byte> png;
            bool out_of_memory{false};
        } encoded;
        // Called from C: no exception may escape.
        const auto append = [](void *context, void *data, int size) noexcept {
            auto &out = *static_cast<Encoded *>(context);
            const auto *bytes = static_cast<const std::byte *>(data);
            try {
                out.png.insert(out.png.end(), bytes, bytes + size);
            } catch (const std::bad_alloc &) {
                out.out_of_memory = true;
            }
        };
        const int stride = size_.width * static_cast<int>(sizeof(Color));
        // stb fails only on allocation.
        if (stbi_write_png_to_func(append, &encoded, size_.width, size_.height, channels, pixels_.data(), stride) == 0
            || encoded.out_of_memory) {
            return std::unexpected{Error::out_of_memory};
        }
        return detail::write_file(path, encoded.png);
    }
}
