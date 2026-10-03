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
#include <memory>
#include <optional>

struct GLFWwindow;

namespace tgx {
    class Texture;
    class Window;
}

namespace tgx::gl {
    class Shader;
    class VertexArray;

    namespace detail {
        class Batch;
        struct DeviceAccess;
    }

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

    // Marks the GL context as usable: GPU resources (gl:: ones, Texture) may
    // be created only while it exists and must be destroyed before it. At most
    // one may exist.
    //
    // It also holds what the Canvas has collected and not drawn yet, and draws
    // that before anything else of its own: a draw, a clear, a viewport
    // change, presenting the frame. So the picture follows the order of the
    // calls. flush() draws it on demand, before raw GL calls.
    //
    // It presents the frames of the window it was created for; the window
    // must outlive it (asserted).
    class Device {
    public:
        // Loads GL functions for the window's context and, on a debug context,
        // routes driver messages to the log. Frames go to this window.
        [[nodiscard]] static auto create(Window &window) noexcept -> Result<Device>;

        Device(const Device &) = delete;
        auto operator=(const Device &) -> Device & = delete;

        Device(Device &&other) noexcept;
        auto operator=(Device &&other) noexcept -> Device &;

        ~Device();

        auto clear(const ClearParams &params) noexcept -> void;

        // Draws what the Canvas has collected.
        auto flush() noexcept -> void;

        // Draws what the Canvas has collected and shows the frame in the
        // window, waiting for the display with vsync on. App::swap_buffers
        // does this.
        auto present() noexcept -> void;

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
        friend struct detail::DeviceAccess;

        Device() noexcept;

        auto release() noexcept -> void;


        // The values GL clears to now, so a clear only sets what changed.
        Color m_clear_color{0, 0, 0, 0};
        float m_clear_depth{1.f};
        std::int32_t m_clear_stencil{0};
        std::array<int, 4> m_viewport{};
        // What GL is set to now; a fresh context starts at the defaults.
        RenderState m_state{};
        // The window frames go to. Its handle stays put when the Window object
        // moves.
        GLFWwindow *m_window{nullptr};
        // Created with the Device; behind a pointer to keep its internals out
        // of this header.
        std::unique_ptr<detail::Batch> m_batch;
        bool m_owned{false};
    };
}
