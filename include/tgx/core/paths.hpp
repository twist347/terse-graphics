#pragma once

#include "tgx/core/error.hpp"

#include <filesystem>
#include <string_view>

namespace tgx {
    // The folder of the running program, for files shipped next to it. Found
    // wherever the program is started from, unlike paths relative to the
    // working directory, which an IDE often sets elsewhere. In a macOS .app,
    // its Contents/MacOS. Empty if the OS does not tell; paths built on it
    // are then relative to the working directory.
    //
    //     auto player = tgx::Texture::load(tgx::app_directory() / "assets/player.png");
    [[nodiscard]] auto app_directory() -> std::filesystem::path;

    // The user's folder for the program's saves and settings, created if
    // missing: organization/application under %APPDATA% on Windows,
    // ~/Library/Application Support on macOS, $XDG_DATA_HOME or
    // ~/.local/share elsewhere. organization may be empty; both are single
    // folder names in UTF-8. Fails with Error::platform without a home folder
    // to put it in, Error::io if it cannot be created.
    //
    //     const auto saves = tgx::save_directory("me", "space game");
    //     if (saves) { const auto saved = shot.save(*saves / "shot.png"); }
    [[nodiscard]] auto save_directory(
        std::string_view organization,
        std::string_view application
    ) -> Result<std::filesystem::path>;
}
