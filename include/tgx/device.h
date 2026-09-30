#pragma once

#include "tgx/color.h"
#include "tgx/error.h"
#include "tgx/size.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

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

    [[nodiscard]] constexpr auto operator&(ClearMask lhs, ClearMask rhs) noexcept -> ClearMask {
        return static_cast<ClearMask>(
            static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs)
        );
    }

    constexpr auto operator|=(ClearMask &lhs, ClearMask rhs) noexcept -> ClearMask & {
        return lhs = lhs | rhs;
    }

    [[nodiscard]] constexpr auto any_of(ClearMask mask, ClearMask bit) noexcept -> bool {
        return (static_cast<std::uint32_t>(mask) & static_cast<std::uint32_t>(bit)) != 0u;
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
        // For count: everything from first to the end of the buffer.
        static constexpr std::size_t all = std::numeric_limits<std::size_t>::max();

        // Vertices, or indices when the vertex array has an index buffer. A
        // count of 0 draws nothing, so an empty batch stays empty.
        std::size_t count{all};
        // First vertex, or first index, to draw from.
        std::size_t first{0};
        Primitive primitive{Primitive::triangles};
    };

    // Owns nothing on the GPU, but marks the GL context as usable: every gl::
    // resource must be destroyed before its Device. At most one may exist.
    class Device {
    public:
        // Loads GL functions for the window's context and, on a debug context,
        // routes driver messages to the log.
        [[nodiscard]] static auto create(Window &window) noexcept -> Result<Device>;

        Device(const Device &) = delete;
        auto operator=(const Device &) -> Device & = delete;

        Device(Device &&other) noexcept;
        auto operator=(Device &&other) noexcept -> Device &;

        ~Device();

        auto set_clear_color(Color color) noexcept -> void;

        auto clear(ClearMask mask = ClearMask::color) noexcept -> void;

        auto set_viewport(int x, int y, int width, int height) noexcept -> void;
        // The whole of a framebuffer of this size, usually framebuffer_size().
        auto set_viewport(Size size) noexcept -> void { set_viewport(0, 0, size.width, size.height); }

        // Draws with the index buffer when the vertex array has one, straight
        // from the vertices otherwise.
        auto draw(
            const gl::Shader &shader,
            const gl::VertexArray &vertices,
            const DrawParams &params = {}
        ) noexcept -> void;

    private:
        Device() noexcept;

        auto release() noexcept -> void;

        Color m_clear_color{};
        std::array<int, 4> m_viewport{};
        bool m_owned{false};
    };
}
