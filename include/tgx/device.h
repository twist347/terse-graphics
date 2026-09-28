#pragma once

#include "tgx/color.h"
#include "tgx/error.h"

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

    [[nodiscard]] constexpr auto any_of(ClearMask mask, ClearMask bit) noexcept -> bool {
        return (static_cast<std::uint32_t>(mask) & static_cast<std::uint32_t>(bit)) != 0U;
    }

    class Device {
    public:
        // Loads GL functions for the window's context and, on a debug context,
        // routes driver messages to stderr.
        [[nodiscard]] static auto create(Window &window) noexcept -> Result<Device>;

        Device(const Device &) = delete;
        auto operator=(const Device &) -> Device & = delete;

        Device(Device &&) noexcept = default;
        auto operator=(Device &&) noexcept -> Device & = default;

        auto set_clear_color(Color color) noexcept -> void;
        auto clear(ClearMask mask = ClearMask::color) noexcept -> void;

        auto set_viewport(int x, int y, int width, int height) noexcept -> void;
    private:
        Device() noexcept = default;

        Color m_clear_color{};
        std::array<int, 4> m_viewport{};
    };
}
