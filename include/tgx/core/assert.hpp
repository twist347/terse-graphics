#pragma once

#include <array>
#include <format>
#include <source_location>
#include <utility>

// CMake defines this for the library and all that link it. Without CMake,
// define it the same way everywhere: inline header code depends on it.
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

    // A stack buffer, so a failing assert never allocates and works in
    // noexcept code. Long messages are cut off.
    template<typename... Ts>
    [[noreturn]] auto assert_failed_fmt(
        const char *expr,
        std::source_location loc,
        std::format_string<Ts...> fmt,
        Ts &&... args
    ) noexcept -> void {
        std::array<char, 512> buf{};
        const auto res = std::format_to_n(buf.data(), buf.size() - 1, fmt, std::forward<Ts>(args)...);
        *res.out = '\0';

        assert_failed(expr, buf.data(), loc);
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
    ((void) sizeof(bool((cond))), (void) sizeof(::std::formatted_size(__VA_ARGS__)))
#endif
