#pragma once

#include <chrono>

namespace tgx {
    class Clock {
    public:
        auto tick() noexcept -> void;

        [[nodiscard]] auto delta() const noexcept -> float { return m_delta; }
        [[nodiscard]] auto elapsed() const noexcept -> float { return m_elapsed; }
        [[nodiscard]] auto fps() const noexcept -> float { return m_fps; }

        [[nodiscard]] auto fps_updated() const noexcept -> bool { return m_fps_updated; }

    private:
        using SteadyClock = std::chrono::steady_clock;

        SteadyClock::time_point m_start{};
        SteadyClock::time_point m_last{};
        SteadyClock::time_point m_window_start{};
        int m_frames{0};
        float m_delta{0.f};
        float m_elapsed{0.f};
        float m_fps{0.f};
        bool m_fps_updated{false};
        bool m_started{false};
    };
}
