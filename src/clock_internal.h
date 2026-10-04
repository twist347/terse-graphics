#pragma once

#include <chrono>
#include <cstddef>

namespace tgx::detail {
    // What Clock shows: the Device's frames, timed as they are presented.
    struct FrameClock {
        using SteadyClock = std::chrono::steady_clock;

        SteadyClock::time_point start{};
        SteadyClock::time_point last{};
        // Where the second fps() is counted over began, and the frames in it.
        SteadyClock::time_point fps_start{};
        std::size_t fps_frames{0};
        float delta{0.f};
        double elapsed{0.0};
        float fps{0.f};
        bool fps_updated{false};
        bool started{false};

        // A frame was presented. The first starts the clock.
        auto tick() noexcept -> void;

        // The next delta counts from now; see Device::restart_clock.
        auto restart() noexcept -> void;
    };
}
