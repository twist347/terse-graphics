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

    // How a draw's colors combine with what the framebuffer already holds.
    // Colors are straight (not premultiplied) unless the mode says otherwise.
    enum class Blend : std::int32_t {
        // Overwrites; alpha has no effect.
        none,
        // src * a + dst * (1 - a): ordinary transparency.
        alpha,
        // src + dst * (1 - a), for colors already multiplied by their alpha.
        premultiplied,
        // src * a + dst: light adding up, as in glows and particles.
        additive,
        // src * dst, faded towards dst as alpha drops: darkening and tinting.
        // Like premultiplied, it wants colors already multiplied by their alpha
        // (Color::premultiplied()); a straight see-through color would come out
        // lighter than dst. Opaque colors are the same either way.
        multiply,
    };

    // Which fragments survive against the depth already stored.
    enum class Depth : std::int32_t {
        // No test: later draws cover earlier ones.
        none,
        less,
        less_equal,
    };

    // Which triangles are dropped by the way they face. Counter-clockwise on
    // screen is the front.
    enum class Cull : std::int32_t {
        none,
        back,
        front,
    };

    enum class Fill : std::int32_t {
        solid,
        // Triangle edges only, for looking at the geometry.
        wireframe,
    };

    // The fixed-function settings of one draw. Every draw states them all, so
    // nothing one draw sets leaks into the next; the Device changes only what
    // differs from the previous draw.
    struct RenderState {
        Blend blend{Blend::none};
        Depth depth{Depth::none};
        // Whether passing fragments store their depth; off for see-through
        // geometry drawn after the opaque. Has no effect without a depth test,
        // as GL then stores no depth at all.
        bool depth_write{true};
        Cull cull{Cull::none};
        Fill fill{Fill::solid};

        [[nodiscard]] constexpr auto operator==(const RenderState &) const noexcept -> bool = default;
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
        RenderState state{};
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
        // What GL is set to now; a fresh context starts at the defaults.
        RenderState m_state{};
        bool m_owned{false};
    };
}
