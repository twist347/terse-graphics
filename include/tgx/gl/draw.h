#pragma once

#include "tgx/blend.h"

#include "tgx/gl/texture_slot.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace tgx {
    class Texture;
}

// What a draw of your own takes (Device::draw), besides the shader and the
// vertex array.
namespace tgx::gl {
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

    // A rectangle of the framebuffer in pixels, from its top-left corner like
    // everything else in tgx (images, texture coordinates, the Canvas), not
    // from the bottom-left as glViewport counts.
    struct Viewport {
        int x{0};
        int y{0};
        int width{0};
        int height{0};

        [[nodiscard]] constexpr auto operator==(const Viewport &) const noexcept -> bool = default;
    };

    // What Device::draw takes besides the shader and the vertices. Like the
    // render state, everything is stated per draw: nothing carries over.
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
        // Empty for the whole framebuffer, which is what a draw usually wants;
        // a part of it for split screens, minimaps and the like.
        std::optional<Viewport> viewport{};
        // By slot, for the shader's samplers to read (gl::TextureSlot); every
        // slot a sampler reads must have one. Empty slots are left as they are.
        //
        //     device.draw(shader, quad, {.textures = {&texture}});
        std::array<const Texture *, max_texture_slots> textures{};
    };
}
