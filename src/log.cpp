#include "log_internal.h"

#include <atomic>
#include <cstdio>

namespace {
    // Problems go to stderr, the rest to stdout. stdout is flushed on every
    // message: it is block-buffered when redirected, and would otherwise land
    // after later stderr output in a shared log. One fprintf per message: stdio
    // locks the stream per call, so lines from different threads do not
    // interleave.
    void default_sink(tgx::LogLevel level, std::string_view message, void *) noexcept {
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

    tgx::LogSink g_sink = default_sink;
    void *g_user = nullptr;
    // Atomic because the level is read on every log call, from any thread.
    std::atomic<tgx::LogLevel> g_level{tgx::LogLevel::warn};
}

namespace tgx {
    auto set_log_sink(LogSink sink, void *user) noexcept -> void {
        g_sink = sink ? sink : default_sink;
        g_user = sink ? user : nullptr;
    }

    auto set_log_level(LogLevel level) noexcept -> void {
        g_level.store(level, std::memory_order_relaxed);
    }

    auto detail::log_enabled(LogLevel level) noexcept -> bool {
        return level != LogLevel::off && level >= g_level.load(std::memory_order_relaxed);
    }

    auto detail::log_write(LogLevel level, std::string_view message) noexcept -> void {
        g_sink(level, message, g_user);
    }
}
