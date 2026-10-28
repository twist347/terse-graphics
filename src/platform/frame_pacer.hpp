#pragma once

#include <chrono>

namespace tgx::detail {
    // Holds the window's frames to an fps limit.
    struct FramePacer {
        using SteadyClock = std::chrono::steady_clock;

        // 0 for none.
        int fps{0};
        // Zero without a limit.
        SteadyClock::duration period{};
        // When the next frame may be shown.
        SteadyClock::time_point next_frame{};

        // 0 lifts the limit. Set back to 0 before the window goes.
        auto set_fps(int limit) noexcept -> void;

        // Waits for the next frame's time. Each frame's time is a period after
        // the last one's, not after its end, so oversleeping does not add up.
        // The OS wakes a thread up to a millisecond or two late, so the last
        // part is spun.
        auto wait() noexcept -> void;
    };
}
