#include "tgx/core/clock.h"

#include "core/clock_internal.h"
#include "core/context.h"

namespace tgx {
    auto Clock::delta() const noexcept -> float {
        return detail::context().clock.delta;
    }

    auto Clock::elapsed() const noexcept -> double {
        return detail::context().clock.elapsed;
    }

    auto Clock::fps() const noexcept -> float {
        return detail::context().clock.fps;
    }

    auto Clock::fps_updated() const noexcept -> bool {
        return detail::context().clock.fps_updated;
    }

    auto detail::FrameClock::tick() noexcept -> void {
        const auto now = SteadyClock::now();
        if (!started) {
            started = true;
            start = now;
            last = now;
            fps_start = now;
        }

        delta = std::chrono::duration<float>(now - last).count();
        elapsed = std::chrono::duration<double>(now - start).count();
        last = now;

        ++fps_frames;
        fps_updated = false;
        if (const auto span = std::chrono::duration<double>(now - fps_start).count(); span >= 1.0) {
            fps = static_cast<float>(static_cast<double>(fps_frames) / span);
            fps_frames = 0;
            fps_start = now;
            fps_updated = true;
        }
    }

    auto detail::FrameClock::restart() noexcept -> void {
        // Before the first frame there is nothing to leave out.
        if (!started) {
            return;
        }
        const auto now = SteadyClock::now();
        last = now;
        fps_start = now;
        fps_frames = 0;
    }
}
