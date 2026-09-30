#pragma once

namespace tgx {
    // Width and height in pixels. Signed, like the windowing and GL APIs these
    // come from and go to.
    struct Size {
        int width{0};
        int height{0};

        // Nothing to draw into, e.g. the framebuffer of a minimized window.
        [[nodiscard]] constexpr auto empty() const noexcept -> bool { return width <= 0 || height <= 0; }

        // Width over height. 1 for an empty size, so a minimized window does not
        // put inf or NaN into a projection.
        [[nodiscard]] constexpr auto aspect() const noexcept -> float {
            return empty() ? 1.f : static_cast<float>(width) / static_cast<float>(height);
        }

        [[nodiscard]] constexpr auto operator==(const Size &) const noexcept -> bool = default;
    };
}
