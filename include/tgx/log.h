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

    // Receives one whole message, without a trailing newline. It is called on
    // the thread that made the tgx call, GL driver messages included: they are
    // delivered synchronously. tgx itself runs on the main thread only.
    using LogSink = void (*)(LogLevel level, std::string_view message, void *user) noexcept;

    // nullptr restores the default sink, which writes warnings and errors to
    // stderr and everything else to stdout. Not
    // synchronised with logging: set it before creating the App.
    auto set_log_sink(LogSink sink, void *user = nullptr) noexcept -> void;

    // Messages below the level are dropped before they are formatted. The
    // default is warn: quiet unless something is wrong. Set info to also see
    // the context the driver gave at startup.
    auto set_log_level(LogLevel level) noexcept -> void;
}

// std::format("{}", level) prints to_str(level); string specs such as width apply.
template<>
struct std::formatter<tgx::LogLevel> : std::formatter<std::string_view> {
    auto format(tgx::LogLevel level, std::format_context &ctx) const {
        return std::formatter<std::string_view>::format(tgx::to_str(level), ctx);
    }
};
