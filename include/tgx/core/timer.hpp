#pragma once

#include "tgx/core/assert.hpp"

#include <algorithm>
#include <cmath>

namespace tgx {
    // Something that happens after or every so many seconds: a cooldown, a
    // wave of enemies, a delay, an animation frame. A plain value; it does not
    // read the Clock, so pausing is not calling tick() and slow motion is a
    // smaller dt.
    //
    //     auto spawn = tgx::Timer::every(3.f);
    //     ...
    //     if (spawn.tick(app->clock().delta())) {
    //         enemies.push_back(make_enemy());
    //     }
    class Timer {
    public:
        // Fires once, seconds after it starts.
        [[nodiscard]] static auto once(float seconds) noexcept -> Timer {
            return Timer{seconds, false, false};
        }

        // Like once(), but starts done, until reset(): a cooldown that lets
        // the first shot go at once.
        //
        //     auto reload = tgx::Timer::once_ready(0.2f);
        //     ...
        //     reload.tick(dt);
        //     if (input.down(tgx::Key::space) && reload.done()) {
        //         shoot();
        //         reload.reset();
        //     }
        [[nodiscard]] static auto once_ready(float seconds) noexcept -> Timer {
            return Timer{seconds, false, true};
        }

        // Fires every seconds. What a tick goes past the period counts toward
        // the next one, so the rate holds at any frame rate; with a period
        // shorter than a frame it fires every tick.
        [[nodiscard]] static auto every(float seconds) noexcept -> Timer {
            return Timer{seconds, true, false};
        }

        // Moves on by dt seconds. True on the tick it fires.
        auto tick(float dt) noexcept -> bool {
            TGX_ASSERT_MSG(dt >= 0.f && std::isfinite(dt), "tick({}): not a time step", dt);

            if (done()) {
                return false;
            }
            elapsed_ += dt;
            if (elapsed_ < period_) {
                return false;
            }
            if (repeat_) {
                elapsed_ = std::fmod(elapsed_, period_);
            } else {
                elapsed_ = period_;
                done_ = true;
            }
            return true;
        }

        // A once timer has fired, and stays so until reset(). Never true for
        // every().
        [[nodiscard]] auto done() const noexcept -> bool { return done_; }

        // 0 at the start of the period, 1 when it fires: for a bar or an
        // animation.
        [[nodiscard]] auto progress() const noexcept -> float { return std::min(elapsed_ / period_, 1.f); }

        // Seconds until it fires; 0 once done.
        [[nodiscard]] auto left() const noexcept -> float { return period_ - elapsed_; }

        [[nodiscard]] auto period() const noexcept -> float { return period_; }

        // Starts over from 0, not done.
        auto reset() noexcept -> void {
            elapsed_ = 0.f;
            done_ = false;
        }

    private:
        Timer(float seconds, bool repeat, bool done) noexcept
            : period_{seconds}, elapsed_{done ? seconds : 0.f}, repeat_{repeat}, done_{done} {
            TGX_ASSERT_MSG(seconds > 0.f && std::isfinite(seconds), "timer of {} seconds", seconds);
        }

        float period_{};
        float elapsed_{};
        bool repeat_{};
        bool done_{};
    };
}
