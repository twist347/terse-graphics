#include "core/file.hpp"

#include <fstream>
#include <ios>
#include <system_error>

namespace {
    // A directory opens fine on some systems and reports a size that is
    // anything but its contents (LLONG_MAX on ext4).
    [[nodiscard]] auto regular_file(const std::filesystem::path &path) -> bool {
        std::error_code err;
        return std::filesystem::is_regular_file(path, err);
    }
}

namespace tgx {
    auto detail::readable(const std::filesystem::path &path) -> bool {
        return regular_file(path) && std::ifstream{path, std::ios::binary}.is_open();
    }

    auto detail::read_file(const std::filesystem::path &path) -> Result<std::vector<std::byte>> {
        if (!regular_file(path)) {
            return std::unexpected{Error::io};
        }

        std::ifstream file{path, std::ios::binary | std::ios::ate};
        if (!file) {
            return std::unexpected{Error::io};
        }

        // Opened at the end, so the position is the size; -1 for a stream that
        // cannot tell.
        const std::streamoff size = file.tellg();
        if (size < 0) {
            return std::unexpected{Error::io};
        }

        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))) {
            return std::unexpected{Error::io};
        }
        return bytes;
    }

    auto detail::write_file(const std::filesystem::path &path, std::span<const std::byte> bytes) -> Result<void> {
        // Written beside it first and put in its place once whole, so a write
        // that fails (a full disk) leaves the old file as it was.
        std::filesystem::path temp = path;
        temp += ".tgx-partial";

        std::ofstream file{temp, std::ios::binary | std::ios::trunc};
        if (!file) {
            return std::unexpected{Error::io};
        }
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        // The last of it reaches the disk only now: a full disk shows here.
        file.close();

        std::error_code err;
        if (file) {
            std::filesystem::rename(temp, path, err);
        }
        if (!file || err) {
            std::filesystem::remove(temp, err);
            return std::unexpected{Error::io};
        }
        return {};
    }
}
