#include "tgx/assert.h"

#include <cstdio>
#include <cstdlib>

namespace tgx::detail {
    void assert_failed(const char *expr, std::string_view msg, std::source_location loc) noexcept {
        std::fprintf(
            stderr,
            "%s:%u: assertion failed in %s\n  %s%s%.*s\n",
            loc.file_name(),
            loc.line(),
            loc.function_name(),
            expr,
            msg.empty() ? "" : "\n  ",
            static_cast<int>(msg.size()),
            msg.data()
        );
        std::fflush(stderr);
        std::abort();
    }
}
