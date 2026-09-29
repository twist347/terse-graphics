#pragma once

#include <chrono>
#include <cstddef>

namespace tgx {
    class Clock {
    public:
        // Marks the end of a frame. A clock that is not running is started
        // first, so that tick measures (almost) nothing.
        auto tick() noexcept -> void;

        // Makes the next delta() count from now, e.g. after loading a level:
        // the time spent before is left out of delta() and fps(). elapsed()
        // keeps counting real time. Starts the clock if it is not running.
        auto restart() noexcept -> void;

        [[nodiscard]] auto started() const noexcept -> bool { return m_started; }

        [[nodiscard]] auto delta() const noexcept -> float { return m_delta; }
        // Seconds since the first tick. A double: a float drops to millisecond
        // steps after a few hours.
        [[nodiscard]] auto elapsed() const noexcept -> double { return m_elapsed; }
        [[nodiscard]] auto fps() const noexcept -> float { return m_fps; }

        [[nodiscard]] auto fps_updated() const noexcept -> bool { return m_fps_updated; }

    private:
        using SteadyClock = std::chrono::steady_clock;

        SteadyClock::time_point m_start{};
        SteadyClock::time_point m_last{};
        SteadyClock::time_point m_window_start{};
        std::size_t m_frames{0};
        float m_delta{0.f};
        double m_elapsed{0.0};
        float m_fps{0.f};
        bool m_fps_updated{false};
        bool m_started{false};
    };
}
