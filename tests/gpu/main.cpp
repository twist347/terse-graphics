// The GPU tests' main: makes the one App before running tests. Listing tests
// and other doctest queries need no display; running needs GL 3.3.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "gpu.hpp"

#include <glad/gl.h>

#include <cstdio>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <utility>

namespace {
    tgx::App *s_app = nullptr;

    // Keep failures inside the test when possible, and fail the process for
    // messages during setup or teardown too. Driver callbacks are synchronous.
    struct GraphicsLog {
        GraphicsLog() {
            current = this;
            tgx::set_log_sink(sink, this);
        }
        GraphicsLog(const GraphicsLog &) = delete;
        auto operator=(const GraphicsLog &) -> GraphicsLog & = delete;
        ~GraphicsLog() {
            if (original_get_error) {
                glad_glGetError = original_get_error;
            }
            tgx::set_log_sink(nullptr);
            current = nullptr;
        }

        auto watch_api_errors() noexcept -> void {
            original_get_error = std::exchange(glad_glGetError, get_error);
        }

        bool failed{false};

    private:
        static auto GLAD_API_PTR get_error() -> GLenum {
            const GLenum error = current->original_get_error();
            if (error != GL_NO_ERROR) {
                current->failed = true;
                if (doctest::is_running_in_test) {
                    FAIL_CHECK("unexpected GL error: ", std::format("{:#x}", error));
                } else {
                    std::println(stderr, "tgx gpu tests: GL error outside a test {:#x}", error);
                }
            }
            return error;
        }

        static auto sink(tgx::LogLevel level, std::string_view message, void *user) noexcept -> void {
            FILE *out = level >= tgx::LogLevel::warn ? stderr : stdout;
            std::fprintf(out, "[tgx] %s: %.*s\n", tgx::to_str(level), static_cast<int>(message.size()), message.data());
            std::fflush(out);
            if (level >= tgx::LogLevel::warn && (message.starts_with("gl ") || message.starts_with("glfw "))) {
                static_cast<GraphicsLog *>(user)->failed = true;
                if (doctest::is_running_in_test) {
                    // FAIL_CHECK records a failure without throwing through
                    // the noexcept log sink and the driver's C callback.
                    FAIL_CHECK("unexpected graphics message: ", std::string{message});
                }
            }
        }

        static inline GraphicsLog *current{nullptr};
        PFNGLGETERRORPROC original_get_error{nullptr};
    };
}

auto tgx_test::app() -> tgx::App & {
    return *s_app;
}

int main(int argc, char **argv) {
    doctest::Context context{argc, argv};
    context.setAsDefaultForAssertsOutOfTestCases();
    if (context.shouldExit() || doctest::getContextOptions()->no_run) {
        return context.run();
    }

    GraphicsLog diagnostics{};
    // The GL info, so a run's log shows which driver it checked.
    tgx::set_log_level(tgx::LogLevel::info);
    int result = 0;
    {
        auto app = tgx::App::create({.size = {640, 360}, .title = "tgx gpu tests", .vsync = false, .debug_context = true});
        if (!app) {
            std::println(stderr, "tgx gpu tests: no window with a GL 3.3 context ({})", app.error());
            return 1;
        }
        s_app = &*app;
        diagnostics.watch_api_errors();
        tgx::set_log_level(tgx::LogLevel::warn);

        result = context.run();
        // Also catch pending API errors on contexts without KHR_debug.
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
            std::println(stderr, "tgx gpu tests: unhandled GL error {:#x}", error);
            diagnostics.failed = true;
        }
    }
    return diagnostics.failed ? 1 : result;
}
