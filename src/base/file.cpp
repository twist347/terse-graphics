#include "base/file.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <system_error>
#include <utility>

namespace {
    // A directory may open fine and report a bogus size (LLONG_MAX on ext4).
    [[nodiscard]] auto regular_file(const std::filesystem::path &path) -> bool {
        std::error_code err{};
        return std::filesystem::is_regular_file(path, err);
    }

    struct PartialFile {
        std::filesystem::path path{};
        std::FILE *file{nullptr};
        bool owned{false};

        PartialFile() noexcept = default;
        PartialFile(const PartialFile &) = delete;
        auto operator=(const PartialFile &) -> PartialFile & = delete;
        auto operator=(PartialFile &&) -> PartialFile & = delete;

        PartialFile(PartialFile &&other) noexcept
            : path{std::move(other.path)}, file{std::exchange(other.file, nullptr)}, owned{std::exchange(other.owned, false)} {
            other.path.clear();
        }

        ~PartialFile() {
            if (!owned) {
                return;
            }
            if (file) {
                std::fclose(file);
            }
            std::error_code ignored{};
            std::filesystem::remove(path, ignored);
        }

        [[nodiscard]] auto commit(const std::filesystem::path &destination) noexcept -> bool {
            if (std::fclose(file) != 0) {
                file = nullptr;
                return false;
            }
            file = nullptr;

            std::error_code err{};
            std::filesystem::rename(path, destination, err);
            if (err) {
                return false;
            }
            owned = false;
            path.clear();
            return true;
        }
    };

    [[nodiscard]] auto open_partial(const std::filesystem::path &destination) -> PartialFile {
        static std::atomic<std::uint64_t> sequence{0};
        const auto stamp = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());

        for (int attempt = 0; attempt < 128; ++attempt) {
            PartialFile partial{};
            partial.path = destination;
            partial.path += std::format(
                ".tgx-partial-{:x}-{:x}", stamp, sequence.fetch_add(1, std::memory_order_relaxed)
            );
            // C11 x: create exclusively, never truncate an existing file.
#ifdef _WIN32
            partial.file = _wfopen(partial.path.c_str(), L"wbx");
#else
            partial.file = std::fopen(partial.path.c_str(), "wbx");
#endif
            if (partial.file) {
                partial.owned = true;
                return partial;
            }
            if (errno != EEXIST) {
                return {};
            }
        }
        return {};
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
        // Written to a file of its own beside it, then moved into place, so a
        // failed write (a full disk) leaves the old file and its siblings unchanged.
        auto partial = open_partial(path);
        if (!partial.file) {
            return std::unexpected{Error::io};
        }
        const auto written = std::fwrite(bytes.data(), 1, bytes.size(), partial.file);
        if (written != bytes.size() || !partial.commit(path)) {
            return std::unexpected{Error::io};
        }
        return {};
    }
}
