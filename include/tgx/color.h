#pragma once

namespace tgx {
    struct Color {
        float r{0.f};
        float g{0.f};
        float b{0.f};
        float a{1.f};

        [[nodiscard]] constexpr  auto operator==(const Color &) const noexcept -> bool = default;
    };

    namespace colors {
        inline constexpr Color black{0.f, 0.f, 0.f};
        inline constexpr Color white{0.f, 0.f, 0.f};
        inline constexpr Color red{1.f, 0.f, 0.f};
        inline constexpr Color green{0.f, 1.f, 0.f};
        inline constexpr Color blue{0.f, 0.f, 1.f};
    }
}