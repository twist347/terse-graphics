#pragma once

#include <cstdint>
#include <string_view>

namespace tgx {
    enum class LogLevel : std::int32_t {
        debug,
        info,
        warn,
        error,
        // Only as a threshold: drops everything.
        off,
    };

    [[nodiscard]] constexpr auto to_str(LogLevel level) noexcept -> const char * {
        switch (level) {
            case LogLevel::debug: return "debug";
            case LogLevel::info: return "info";
            case LogLevel::warn: return "warn";
            case LogLevel::error: return "error";
            case LogLevel::off: return "off";
        }
        return "unknown";
    }

    // Receives one whole message, without a trailing newline. It may be called
    // from a driver thread, so it must be thread-safe if the app has several.
    using LogSink = void (*)(LogLevel level, std::string_view message, void *user) noexcept;

    // nullptr restores the default sink, which writes warnings and errors to
    // stderr and everything else to stdout. Not
    // synchronised with logging: set it before creating the App.
    auto set_log_sink(LogSink sink, void *user = nullptr) noexcept -> void;

    // Messages below the level are dropped before they are formatted.
    auto set_log_level(LogLevel level) noexcept -> void;
}
