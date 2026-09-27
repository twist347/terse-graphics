#include "tgx/clock.h"

namespace tgx {
    void Clock::tick() noexcept {
        const auto now = SteadyClock::now();

        if (!m_started) {
            m_started = true;
            m_start = now;
            m_last = now;
            m_window_start = now;
        }

        m_delta = std::chrono::duration<float>(now - m_last).count();
        m_elapsed = std::chrono::duration<float>(now - m_start).count();
        m_last = now;

        ++m_frames;
        m_fps_updated = false;

        if (const auto span = std::chrono::duration<double>(now - m_window_start).count(); span >= 1.0) {
            m_fps = static_cast<float>(static_cast<double>(m_frames) / span);
            m_frames = 0;
            m_window_start = now;
            m_fps_updated = true;
        }
    }
}
