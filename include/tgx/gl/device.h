#pragma once

#include "tgx/blend.h"
#include "tgx/color.h"
#include "tgx/error.h"
#include "tgx/size.h"

#include "tgx/gl/texture_slot.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace tgx {
    class Texture;
    class Window;
}

namespace tgx::gl {
    class Shader;
    class VertexArray;

    // What a clear resets, and to what; what is left empty stays as it is.
    // Like a draw, every clear states it all, so nothing carries over from
    // one clear to the next.
    //
    //     device.clear({.color = colors::black, .depth = 1.f});
    struct ClearParams {
        std::optional<Color> color{};
        // Usually 1, the far end of the depth range.
        std::optional<float> depth{};
        std::optional<std::int32_t> stencil{};
    };

    // How consecutive vertices are assembled into primitives.
    enum class Primitive : std::int32_t {
        triangles,
        triangle_strip,
        lines,
        line_strip,
        points
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
        // By slot, for the shader's samplers to read (gl::TextureSlot); every
        // slot a sampler reads must have one. Empty slots are left as they are.
        //
        //     device.draw(shader, quad, {.textures = {&texture}});
        std::array<const Texture *, max_texture_slots> textures{};
    };

    // The GL context, ready to draw: GPU resources (gl:: ones, Texture) are
    // created while it exists and destroyed before it goes. There is one, of
    // the one window; it only sets the context up and tears it down, which is
    // why it can be moved freely and holds nothing else.
    //
    // It also draws what the Canvas has collected before anything else of its
    // own: a draw, a clear, a viewport change, presenting the frame. So the
    // picture follows the order of the calls. flush() draws it on demand,
    // before raw GL calls.
    //
    // It presents the frames of the window it was created for, which must
    // outlive it.
    class Device {
    public:
        // Loads GL functions for the window's context and, on a debug context,
        // routes driver messages to the log. Frames go to this window.
        [[nodiscard]] static auto create(Window &window) noexcept -> Result<Device>;

        Device(const Device &) = delete;
        auto operator=(const Device &) -> Device & = delete;

        Device(Device &&other) noexcept : m_owned{std::exchange(other.m_owned, false)} {
        }

        auto operator=(Device &&other) noexcept -> Device &;

        ~Device();

        auto clear(const ClearParams &params) noexcept -> void;

        // Draws what the Canvas has collected.
        auto flush() noexcept -> void;

        // Draws what the Canvas has collected and shows the frame in the
        // window, waiting for the display with vsync on. App::swap_buffers
        // does this.
        auto present() noexcept -> void;

        // Whether present() waits for the display: no tearing, and frames
        // paced by it. Starts as WindowParams::vsync.
        auto set_vsync(bool enabled) noexcept -> void;

        auto set_viewport(int x, int y, int width, int height) noexcept -> void;
        // The whole of a framebuffer of this size, usually framebuffer_size().
        auto set_viewport(Size size) noexcept -> void { set_viewport(0, 0, size.width, size.height); }

        // Draws with the index buffer when the vertex array has one, straight
        // from the vertices otherwise.
        auto draw(
            const Shader &shader,
            const VertexArray &vertices,
            const DrawParams &params = {}
        ) noexcept -> void;

    private:
        Device() noexcept : m_owned{true} {
        }

        auto release() noexcept -> void;

        // False once moved from: the context is someone else's to tear down.
        bool m_owned{false};
    };
}
