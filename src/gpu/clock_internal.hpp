#pragma once

#include <chrono>
#include <cstddef>

namespace tgx::detail {
    // The data behind Clock.
    struct FrameClock {
        using SteadyClock = std::chrono::steady_clock;

        SteadyClock::time_point start{};
        SteadyClock::time_point last{};
        // The second fps() is counted over: its start and frames so far.
        SteadyClock::time_point fps_start{};
        std::size_t fps_frames{0};
        float delta{0.f};
        double elapsed{0.0};
        float fps{0.f};
        bool fps_updated{false};
        bool started{false};

        // After each present. The first call starts the clock.
        auto tick() noexcept -> void;

        // See Device::restart_clock.
        auto restart() noexcept -> void;
    };
}
