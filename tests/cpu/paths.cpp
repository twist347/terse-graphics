// The program's folder and the user's folder for saves.

#include "tgx/core/paths.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace {
    namespace fs = std::filesystem;

    // An organization folder no other run uses, removed with all inside it
    // when it goes out of scope.
    struct TempOrganization {
        std::string name{std::format("tgx_tests_{:08x}", std::random_device{}())};
        fs::path path{};

        TempOrganization() = default;
        TempOrganization(const TempOrganization &) = delete;
        auto operator=(const TempOrganization &) -> TempOrganization & = delete;
        ~TempOrganization() {
            if (!path.empty()) {
                std::error_code ignored{};
                fs::remove_all(path, ignored);
            }
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
    TempOrganization organization{};
    const auto saves = tgx::save_directory(organization.name, "game one");
    REQUIRE(saves.has_value());
    organization.path = saves->parent_path();

    CHECK(saves->is_absolute());
    CHECK(fs::is_directory(*saves));
    CHECK(saves->filename() == "game one");
    CHECK(saves->parent_path().filename() == organization.name);

    // Again: already there.
    const auto again = tgx::save_directory(organization.name, "game one");
    REQUIRE(again.has_value());
    CHECK(*again == *saves);
}

TEST_CASE("save_directory: no organization") {
    TempOrganization application{};
    const auto saves = tgx::save_directory("", application.name);
    REQUIRE(saves.has_value());
    application.path = *saves;

    CHECK(fs::is_directory(*saves));
    CHECK(saves->filename() == application.name);
}

#if defined(__linux__)
namespace {
    // Sets an environment variable while it lives, then puts the old value
    // back.
    struct ScopedVariable {
        const char *name{};
        std::string old{};
        bool had{false};

        ScopedVariable(const char *name, const char *value) : name{name} {
            if (const char *current = std::getenv(name)) {
                old = current;
                had = true;
            }
            setenv(name, value, 1);
        }
        ScopedVariable(const ScopedVariable &) = delete;
        auto operator=(const ScopedVariable &) -> ScopedVariable & = delete;
        ~ScopedVariable() {
            if (had) {
                setenv(name, old.c_str(), 1);
            } else {
                unsetenv(name);
            }
        }
    };

    // A temp folder removed with all inside it when it goes out of scope.
    struct TempFolder {
        fs::path path{};

        explicit TempFolder(std::string_view name) : path{tgx_test::temp_path(name)} {}
        TempFolder(const TempFolder &) = delete;
        auto operator=(const TempFolder &) -> TempFolder & = delete;
        ~TempFolder() {
            std::error_code ignored{};
            fs::remove_all(path, ignored);
        }
    };
}

TEST_CASE("save_directory: $XDG_DATA_HOME when absolute, else ~/.local/share") {
    const TempFolder data_home{"data_home"};
    const TempFolder home{"home"};
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
