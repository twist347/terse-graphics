#pragma once

#include <format>
#include <source_location>
#include <string_view>

#if !defined(TGX_ENABLE_ASSERTS)
    #if defined(NDEBUG)
        #define TGX_ENABLE_ASSERTS 0
    #else
        #define TGX_ENABLE_ASSERTS 1
    #endif
#endif

namespace tgx::detail {
    [[noreturn]] void assert_failed(
        const char *expr,
        std::string_view msg,
        std::source_location loc = std::source_location::current()
    ) noexcept;
}

#if TGX_ENABLE_ASSERTS
#define TGX_ASSERT(cond) \
    ((cond) ? void(0) : ::tgx::detail::assert_failed(#cond, {}))
#define TGX_ASSERT_MSG(cond, ...) \
    ((cond) ? void(0) : ::tgx::detail::assert_failed(#cond, std::format(__VA_ARGS__)))
#else
    #define TGX_ASSERT(cond)          ((void) sizeof(bool((cond))))
    #define TGX_ASSERT_MSG(cond, ...) ((void) sizeof(bool((cond))))
#endif