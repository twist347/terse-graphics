#include "tgx/core/log.hpp"

#include "core/log_internal.hpp"

#include <atomic>
#include <cstdio>

namespace {
    // stdout is flushed every message: redirected, it is block-buffered and
    // would land after later stderr output. One fprintf per message, so
    // stdio's per-call lock keeps threads' lines apart.
    auto default_sink(tgx::LogLevel level, std::string_view message, void *) noexcept -> void {
        const bool problem = level >= tgx::LogLevel::warn;
        FILE *out = problem ? stderr : stdout;

        std::fprintf(
            out,
            "[tgx] %s: %.*s\n",
            tgx::to_str(level),
            static_cast<int>(message.size()),
            message.data()
        );
        if (!problem) {
            std::fflush(stdout);
        }
    }

    tgx::LogSink s_sink = default_sink;
    void *s_user = nullptr;
    // Every log call reads it, from any thread.
    std::atomic<tgx::LogLevel> s_level{tgx::LogLevel::warn};
}

namespace tgx {
    auto set_log_sink(LogSink sink, void *user) noexcept -> void {
        s_sink = sink ? sink : default_sink;
        s_user = sink ? user : nullptr;
    }

    auto set_log_level(LogLevel level) noexcept -> void {
        s_level.store(level, std::memory_order_relaxed);
    }

    auto log_level() noexcept -> LogLevel {
        return s_level.load(std::memory_order_relaxed);
    }

    auto detail::log_enabled(LogLevel level) noexcept -> bool {
        return level != LogLevel::off && level >= s_level.load(std::memory_order_relaxed);
    }

    auto detail::log_write(LogLevel level, std::string_view message) noexcept -> void {
        s_sink(level, message, s_user);
    }
}
