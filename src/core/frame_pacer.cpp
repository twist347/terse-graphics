#include "core/frame_pacer.hpp"

#include <chrono>
#include <thread>

#if defined(_WIN32)
// From winmm, declared here as raylib does: <windows.h> brings macros such
// as min and max along.
extern "C" __declspec (dllimport) unsigned int __stdcall timeBeginPeriod(unsigned int period);
extern "C" __declspec (dllimport) unsigned int __stdcall timeEndPeriod(unsigned int period);
#endif

namespace {
    // Windows wakes a sleeping thread on a 15.6 ms tick unless asked for
    // 1 ms. That speeds the tick for the whole system, so only while a limit
    // is set.
    auto set_fine_timer([[maybe_unused]] bool fine) noexcept -> void {
#if defined(_WIN32)
        if (fine) {
            timeBeginPeriod(1);
        } else {
            timeEndPeriod(1);
        }
#endif
    }
}

namespace tgx::detail {
    auto FramePacer::set_fps(int limit) noexcept -> void {
        if ((limit > 0) != (fps > 0)) {
            set_fine_timer(limit > 0);
        }
        fps = limit;
        period = limit > 0
                     ? std::chrono::duration_cast<SteadyClock::duration>(std::chrono::duration<double>(1.0 / limit))
                     : SteadyClock::duration{};
        next_frame = {};
    }

    auto FramePacer::wait() noexcept -> void {
        if (period == SteadyClock::duration{}) {
            return;
        }
        const auto now = SteadyClock::now();
        // First frame, or one late by more than a period: start over from
        // now rather than rush to catch up.
        if (next_frame == SteadyClock::time_point{} || now - next_frame > period) {
            next_frame = now + period;
            return;
        }
        constexpr auto spin = std::chrono::milliseconds{2};
        if (next_frame - now > spin) {
            std::this_thread::sleep_until(next_frame - spin);
        }
        while (SteadyClock::now() < next_frame) {
            std::this_thread::yield();
        }
        next_frame += period;
    }
}
