#pragma once

#include "tgx/core/log.hpp"

#include <format>
#include <string_view>
#include <utility>

namespace tgx::detail {
    [[nodiscard]] auto log_enabled(LogLevel level) noexcept -> bool;

    auto log_write(LogLevel level, std::string_view message) noexcept -> void;

    // A stack buffer, like assert_failed_fmt: no allocation, one piece for
    // the sink. Long messages are cut off.
    template<typename... Ts>
    auto log(LogLevel level, std::format_string<Ts...> fmt, Ts &&... args) noexcept -> void {
        if (!log_enabled(level)) {
            return;
        }

        char buf[1024]{};
        const auto res = std::format_to_n(buf, sizeof(buf), fmt, std::forward<Ts>(args)...);
        log_write(level, std::string_view{buf, res.out});
    }

    template<typename... Ts>
    auto log_debug(std::format_string<Ts...> fmt, Ts &&... args) noexcept -> void {
        log(LogLevel::debug, fmt, std::forward<Ts>(args)...);
    }

    template<typename... Ts>
    auto log_info(std::format_string<Ts...> fmt, Ts &&... args) noexcept -> void {
        log(LogLevel::info, fmt, std::forward<Ts>(args)...);
    }

    template<typename... Ts>
    auto log_warn(std::format_string<Ts...> fmt, Ts &&... args) noexcept -> void {
        log(LogLevel::warn, fmt, std::forward<Ts>(args)...);
    }

    template<typename... Ts>
    auto log_error(std::format_string<Ts...> fmt, Ts &&... args) noexcept -> void {
        log(LogLevel::error, fmt, std::forward<Ts>(args)...);
    }
}
