#pragma once

namespace tgx {
    class Device;

    // The time of the frames the Device presents, as of the last present(): a
    // frame lasts from one present() to the next. Reached through
    // device().clock() or app->clock(); like the Device, it only reads state
    // kept in one place inside tgx, and counts from the first present().
    class Clock {
    public:
        Clock(const Clock &) = delete;
        auto operator=(const Clock &) -> Clock & = delete;

        // Seconds the last frame took; 0 for the first, which has no frame
        // before it.
        [[nodiscard]] auto delta() const noexcept -> float;

        // Seconds since the first present(). A double: a float drops to
        // millisecond steps after a few hours.
        [[nodiscard]] auto elapsed() const noexcept -> double;

        // Frames per second over the last second; 0 during the first one.
        [[nodiscard]] auto fps() const noexcept -> float;

        // Whether the last present() updated fps(), which it does once a
        // second.
        [[nodiscard]] auto fps_updated() const noexcept -> bool;

    private:
        friend class Device;

        Clock() noexcept = default;
    };
}
