#pragma once

#include <format>
#include <source_location>
#include <utility>

#if !defined(TGX_ENABLE_ASSERTS)
    #if defined(NDEBUG)
        #define TGX_ENABLE_ASSERTS 0
    #else
        #define TGX_ENABLE_ASSERTS 1
    #endif
#endif

namespace tgx::detail {
    [[noreturn]] auto assert_failed(
        const char *expr,
        const char *msg,
        std::source_location loc = std::source_location::current()
    ) noexcept -> void;

    // Formats into a stack buffer, so a failing assert never needs an allocation
    // to report itself and stays usable from noexcept code. Long messages are
    // truncated.
    template<typename... Ts>
    [[noreturn]] auto assert_failed_fmt(
        const char *expr,
        std::source_location loc,
        std::format_string<Ts...> fmt,
        Ts &&... args
    ) noexcept -> void {
        char buf[512];
        const auto res = std::format_to_n(buf, sizeof(buf) - 1, fmt, std::forward<Ts>(args)...);
        *res.out = '\0';

        assert_failed(expr, buf, loc);
    }
}

#if TGX_ENABLE_ASSERTS
#define TGX_ASSERT(cond) \
    ((cond) ? void(0) : ::tgx::detail::assert_failed(#cond, nullptr))
#define TGX_ASSERT_MSG(cond, ...) \
    ((cond) ? void(0) : ::tgx::detail::assert_failed_fmt( \
        #cond, std::source_location::current(), __VA_ARGS__))
#else
#define TGX_ASSERT(cond) ((void) sizeof(bool((cond))))
#define TGX_ASSERT_MSG(cond, ...) \
    ((void) sizeof(bool((cond))), (void) sizeof(::std::format(__VA_ARGS__)))
#endif
