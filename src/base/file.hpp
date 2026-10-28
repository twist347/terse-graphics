#pragma once

#include "tgx/core/error.hpp"

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace tgx::detail {
    // A regular file that opens for reading: the check behind Error::io, for
    // code that opens the file another way.
    [[nodiscard]] auto readable(const std::filesystem::path &path) -> bool;

    // The whole file, or Error::io. A stream, not stdio, which cannot open
    // non-ASCII paths on Windows. Image and Sound decode from this.
    [[nodiscard]] auto read_file(const std::filesystem::path &path) -> Result<std::vector<std::byte>>;

    // Creates or replaces the file. On Error::io an old file is left
    // unchanged.
    [[nodiscard]] auto write_file(const std::filesystem::path &path, std::span<const std::byte> bytes) -> Result<void>;
}
