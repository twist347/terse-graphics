#pragma once

#include "tgx/core/error.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace tgx::detail {
    // Whether the path is a regular file that opens for reading: what
    // Error::io means, for what opens the file another way.
    [[nodiscard]] auto readable(const std::filesystem::path &path) -> bool;

    // The whole file, or Error::io if it cannot be opened or read through.
    // Read through a stream rather than stdio, which cannot open non-ASCII
    // paths on Windows; what loads assets (Image, Sound) decodes from this.
    [[nodiscard]] auto read_file(const std::filesystem::path &path) -> Result<std::vector<std::byte>>;

    // The bytes as the whole file, made or replaced; Error::io if it cannot
    // be written through.
    [[nodiscard]] auto write_file(const std::filesystem::path &path, std::span<const std::byte> bytes) -> Result<void>;
}
