#pragma once

// What the GL context is, and GL's own error reports, in the log.
namespace tgx::detail {
    // The version, renderer, limits. Needs a loaded context.
    auto log_context_info() noexcept -> void;

    // Turns on KHR_debug where the context has it, so the driver's errors
    // and warnings go to the log. Logged last in the context info: with the
    // callback off, driver silence means nothing.
    auto install_debug_callback() noexcept -> void;
}
