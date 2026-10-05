#pragma once

#include "tgx/error.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace tgx::detail {
    // The whole file, or Error::io if it cannot be opened or read through.
    // Read through a stream rather than stdio, which cannot open non-ASCII
    // paths on Windows; what loads assets (Image, Sound) decodes from this.
    [[nodiscard]] auto read_file(const std::filesystem::path &path) -> Result<std::vector<std::byte>>;
}
