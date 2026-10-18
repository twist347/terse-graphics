#include "core/file.hpp"

#include <fstream>
#include <ios>
#include <system_error>

namespace {
    // A directory may open fine and report a bogus size (LLONG_MAX on ext4).
    [[nodiscard]] auto regular_file(const std::filesystem::path &path) -> bool {
        std::error_code err{};
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

        // Opened at the end, so this is the size; -1 if unknown.
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
        // Written next to it, then moved into place, so a failed write (a
        // full disk) leaves the old file unchanged.
        std::filesystem::path temp = path;
        temp += ".tgx-partial";

        std::ofstream file{temp, std::ios::binary | std::ios::trunc};
        if (!file) {
            return std::unexpected{Error::io};
        }
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        // The last bytes are written only now, so a full disk shows here.
        file.close();

        if (file) {
            std::error_code err{};
            std::filesystem::rename(temp, path, err);
            if (!err) {
                return {};
            }
        }
        std::error_code ignored{};
        std::filesystem::remove(temp, ignored);
        return std::unexpected{Error::io};
    }
}
