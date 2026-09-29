#pragma once

#include "tgx/color.h"
#include "tgx/error.h"

#include <array>
#include <cstdint>

namespace tgx {
    class Window;

    namespace gl {
        class Shader;
        class VertexArray;
    }

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

    // How consecutive vertices are assembled into primitives.
    enum class Primitive : std::int32_t {
        triangles,
        triangle_strip,
        lines,
        line_strip,
        points
    };

    struct DrawParams {
        // Vertices, or indices when the vertex array has an index buffer.
        std::uint32_t count{0};
        // First vertex, or first index, to draw from.
        std::uint32_t first{0};
        Primitive primitive{Primitive::triangles};
    };

    class Device {
    public:
        // Loads GL functions for the window's context and, on a debug context,
        // routes driver messages to the log.
        [[nodiscard]] static auto create(Window &window) noexcept -> Result<Device>;

        Device(const Device &) = delete;
        auto operator=(const Device &) -> Device & = delete;

        Device(Device &&) noexcept = default;
        auto operator=(Device &&) noexcept -> Device & = default;

        auto set_clear_color(Color color) noexcept -> void;

        auto clear(ClearMask mask = ClearMask::color) noexcept -> void;

        auto set_viewport(int x, int y, int width, int height) noexcept -> void;

        // Draws with the index buffer when the vertex array has one, straight
        // from the vertices otherwise.
        auto draw(
            const gl::Shader &shader,
            const gl::VertexArray &vertices,
            const DrawParams &params
        ) noexcept -> void;

    private:
        Device() noexcept = default;

        Color m_clear_color{};
        std::array<int, 4> m_viewport{};
    };
}
