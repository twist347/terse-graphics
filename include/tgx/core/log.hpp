#pragma once

#include <cstdint>
#include <format>
#include <string_view>

namespace tgx {
    enum class LogLevel : std::int32_t {
        debug,
        info,
        warn,
        error,
        // Only a threshold: drops everything.
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

    // Gets one whole message, without a trailing newline, on the thread that
    // made the tgx call. GL driver messages too: they are synchronous.
    using LogSink = void (*)(LogLevel level, std::string_view message, void *user) noexcept;

    // nullptr restores the default: warnings and errors to stderr, the rest
    // to stdout. Not synchronized, so set it before creating the App.
    auto set_log_sink(LogSink sink, void *user = nullptr) noexcept -> void;

    // Lower messages are dropped before formatting. Default warn: quiet
    // unless something is wrong. info also logs at startup what tgx runs on:
    // versions, windowing system, window sizes, monitor, working directory,
    // GL context and audio output.
    auto set_log_level(LogLevel level) noexcept -> void;

    [[nodiscard]] auto log_level() noexcept -> LogLevel;
}

// Prints to_str(level); string specs such as width work.
template<>
struct std::formatter<tgx::LogLevel> : std::formatter<std::string_view> {
    auto format(tgx::LogLevel level, std::format_context &ctx) const {
        return std::formatter<std::string_view>::format(tgx::to_str(level), ctx);
    }
};
