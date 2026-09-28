#pragma once

// Bookkeeping between Platform and Window that is not part of the public API.
namespace tgx::detail {
    auto window_opened() noexcept -> void;
    auto window_closed() noexcept -> void;
}
