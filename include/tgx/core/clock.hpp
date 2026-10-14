#pragma once

namespace tgx {
    class Device;

    // Timing of presented frames, as of the last present(). A frame lasts
    // from one present() to the next; counting starts at the first. Get it
    // with device().clock() or app->clock().
    class Clock {
    public:
        Clock(const Clock &) = delete;
        auto operator=(const Clock &) -> Clock & = delete;

        // Seconds. 0 for the first frame.
        [[nodiscard]] auto delta() const noexcept -> float;

        // Seconds. A float would drop to millisecond steps after a few hours.
        [[nodiscard]] auto elapsed() const noexcept -> double;

        // Measured over the last second. 0 during the first.
        [[nodiscard]] auto fps() const noexcept -> float;

        // Whether the last present() updated fps(), which happens once a
        // second.
        [[nodiscard]] auto fps_updated() const noexcept -> bool;

    private:
        friend class Device;

        Clock() noexcept = default;
    };
}
