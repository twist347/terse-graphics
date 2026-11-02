// The program's folder and the user's folder for saves.

#include "tgx/core/paths.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
    namespace fs = std::filesystem;

    // Changes only this process's environment, restoring it on scope exit.
    struct ScopedVariable {
        using Char = fs::path::value_type;
        const Char *name{};
        fs::path::string_type old{};
        bool had{false};

        ScopedVariable(const Char *name, const Char *value) : name{name} {
#if defined(_WIN32)
            SetLastError(ERROR_SUCCESS);
            const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
            had = size != 0 || GetLastError() == ERROR_SUCCESS;
            if (size != 0) {
                old.resize(size);
                const DWORD length = GetEnvironmentVariableW(name, old.data(), size);
                REQUIRE(length < size);
                old.resize(length);
            }
            REQUIRE(SetEnvironmentVariableW(name, value) != 0);
#else
            const Char *current = std::getenv(name);
            if (current) {
                old = current;
                had = true;
            }
            REQUIRE(setenv(name, value, 1) == 0);
#endif
        }
        ScopedVariable(const ScopedVariable &) = delete;
        auto operator=(const ScopedVariable &) -> ScopedVariable & = delete;
        ~ScopedVariable() {
#if defined(_WIN32)
            SetEnvironmentVariableW(name, had ? old.c_str() : nullptr);
#else
            if (had) {
                setenv(name, old.c_str(), 1);
            } else {
                unsetenv(name);
            }
#endif
        }
    };

    // The variable is restored before the temporary folder is removed.
    struct ScopedDataHome {
        tgx_test::TempFile directory{"save_directory"};
#if defined(_WIN32)
        ScopedVariable variable{L"APPDATA", directory.path.c_str()};
#elif defined(__APPLE__)
        ScopedVariable variable{"HOME", directory.path.c_str()};
#else
        ScopedVariable variable{"XDG_DATA_HOME", directory.path.c_str()};
#endif

        [[nodiscard]] auto base() const -> fs::path {
#if defined(__APPLE__)
            return directory.path / "Library" / "Application Support";
#else
            return directory.path;
#endif
        }
    };
}

TEST_CASE("app_directory: the folder this test program is in") {
    const fs::path directory = tgx::app_directory();
    REQUIRE(!directory.empty());
    CHECK(directory.is_absolute());

    bool found = false;
    for (const auto &entry: fs::directory_iterator(directory)) {
        found = found || entry.path().stem() == "tgx_tests_cpu";
    }
    CHECK(found);
}

TEST_CASE("save_directory: organization/application, created") {
    const ScopedDataHome data_home{};
    const auto saves = tgx::save_directory("org", "game one");
    REQUIRE(saves.has_value());

    CHECK(*saves == data_home.base() / "org" / "game one");
    CHECK(saves->is_absolute());
    CHECK(fs::is_directory(*saves));
    CHECK(saves->filename() == "game one");
    CHECK(saves->parent_path().filename() == "org");

    // Again: already there.
    const auto again = tgx::save_directory("org", "game one");
    REQUIRE(again.has_value());
    CHECK(*again == *saves);
}

TEST_CASE("save_directory: no organization") {
    const ScopedDataHome data_home{};
    const auto saves = tgx::save_directory("", "app");
    REQUIRE(saves.has_value());

    CHECK(*saves == data_home.base() / "app");
    CHECK(fs::is_directory(*saves));
    CHECK(saves->filename() == "app");
}

#if defined(__linux__)
TEST_CASE("save_directory: $XDG_DATA_HOME when absolute, else ~/.local/share") {
    const tgx_test::TempFile data_home{"data_home"};
    const tgx_test::TempFile home{"home"};
    const ScopedVariable home_variable{"HOME", home.path.c_str()};

    {
        const ScopedVariable data_home_variable{"XDG_DATA_HOME", data_home.path.c_str()};
        const auto saves = tgx::save_directory("org", "app");
        REQUIRE(saves.has_value());
        CHECK(*saves == data_home.path / "org" / "app");
    }
    {
        // Relative: ignored, as the spec says.
        const ScopedVariable data_home_variable{"XDG_DATA_HOME", "relative/data"};
        const auto saves = tgx::save_directory("org", "app");
        REQUIRE(saves.has_value());
        CHECK(*saves == home.path / ".local" / "share" / "org" / "app");
    }
}
#endif
