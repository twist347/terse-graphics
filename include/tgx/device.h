#pragma once

#include "tgx/color.h"

#include <array>
#include <cstdint>

namespace tgx {
    class Window;

    enum class ClearMask : std::uint32_t {
        none = 0u,
        color = 1u << 0u,
        depth = 1u << 1u,
        stencil = 1u << 2u,
    };

    [[nodiscard]] constexpr auto operator|(ClearMask lhs, ClearMask rhs) noexcept -> ClearMask {
        return static_cast<ClearMask>(
            static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs)
        );
    }

    [[nodiscard]] constexpr auto has(ClearMask mask, ClearMask bit) noexcept -> bool {
        return (static_cast<std::uint32_t>(mask) & static_cast<std::uint32_t>(bit)) != 0U;
    }

    class Device {
    public:
        explicit Device(Window &window) noexcept;

        void set_clear_color(const Color &color) noexcept;
        void clear(ClearMask mask = ClearMask::color) noexcept;

        void set_viewport(int x, int y, int width, int height) noexcept;
    private:
        Color m_clear_color{};
        std::array<int, 4> m_viewport{};
    };
}
