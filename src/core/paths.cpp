#include "tgx/core/paths.hpp"

#include "tgx/core/assert.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace {
    namespace fs = std::filesystem;

    // The running program's file, or empty.
    [[nodiscard]] auto executable() -> fs::path {
#if defined(_WIN32)
        // Longer paths are cut off to the buffer, which then grows.
        std::wstring buffer(MAX_PATH, L'\0');
        while (true) {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0) {
                return {};
            }
            if (length < buffer.size()) {
                buffer.resize(length);
                return buffer;
            }
            buffer.resize(buffer.size() * 2);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return {};
        }
        // The path the program was started by, maybe through a link.
        std::error_code error{};
        fs::path path = fs::canonical(buffer.c_str(), error);
        return error ? fs::path{} : path;
#elif defined(__linux__)
        std::error_code error{};
        fs::path path = fs::read_symlink("/proc/self/exe", error);
        return error ? fs::path{} : path;
#else
        return {};
#endif
    }

    // A UTF-8 name as a path, also where paths are not UTF-8 (Windows).
    [[nodiscard]] auto utf8_path(std::string_view text) -> fs::path {
        return fs::path{std::u8string{text.begin(), text.end()}};
    }

    // One folder's name: not empty, no separators, not . or .. Unused with
    // asserts off.
    [[maybe_unused]] [[nodiscard]] auto folder_name(std::string_view name) noexcept -> bool {
        return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\") == std::string_view::npos;
    }

    // Where programs keep the user's data, or empty.
    [[nodiscard]] auto user_data() -> fs::path {
#if defined(_WIN32)
        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
        // Too short: the size needed, terminator included.
        if (length >= buffer.size()) {
            buffer.resize(length);
            length = GetEnvironmentVariableW(L"APPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
        }
        if (length == 0 || length >= buffer.size()) {
            return {};
        }
        buffer.resize(length);
        return buffer;
#else
#if !defined(__APPLE__)
        // Relative values are to be ignored, says the XDG spec.
        if (const char *data_home = std::getenv("XDG_DATA_HOME"); data_home && fs::path{data_home}.is_absolute()) {
            return data_home;
        }
#endif
        const char *home = std::getenv("HOME");
        if (!home || *home == '\0') {
            return {};
        }
#if defined(__APPLE__)
        return fs::path{home} / "Library" / "Application Support";
#else
        return fs::path{home} / ".local" / "share";
#endif
#endif
    }
}

namespace tgx {
    auto app_directory() -> std::filesystem::path {
        return executable().parent_path();
    }

    auto save_directory(std::string_view organization, std::string_view application) -> Result<std::filesystem::path> {
        TGX_ASSERT_MSG(
            (organization.empty() || folder_name(organization)) && folder_name(application),
            "save directory '{}/{}': not single folder names",
            organization, application
        );

        fs::path path = user_data();
        if (path.empty()) {
            return std::unexpected{Error::platform};
        }
        if (!organization.empty()) {
            path /= utf8_path(organization);
        }
        path /= utf8_path(application);

        std::error_code error{};
        fs::create_directories(path, error);
        if (error || !fs::is_directory(path, error)) {
            return std::unexpected{Error::io};
        }
        return path;
    }
}
