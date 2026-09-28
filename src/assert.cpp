#include "tgx/assert.h"

#include <cstdio>
#include <cstdlib>

namespace tgx::detail {
    auto assert_failed(const char *expr, const char *msg, std::source_location loc) noexcept -> void {
        const bool has_msg = msg != nullptr && *msg != '\0';

        std::fprintf(
            stderr,
            "%s:%u: assertion failed in %s\n  %s%s%s\n",
            loc.file_name(),
            loc.line(),
            loc.function_name(),
            expr,
            has_msg ? "\n  " : "",
            has_msg ? msg : ""
        );
        std::fflush(stderr);
        std::abort();
    }
}
