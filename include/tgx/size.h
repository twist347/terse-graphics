#pragma once

namespace tgx {
    // Width and height in pixels. Signed, like the windowing and GL APIs these
    // come from and go to.
    struct Size {
        int width{0};
        int height{0};

        [[nodiscard]] constexpr auto operator==(const Size &) const noexcept -> bool = default;
    };
}
