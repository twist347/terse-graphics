#pragma once

#include "tgx/core/blend.hpp"

#include "tgx/gl/texture_slot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace tgx {
    class RenderTarget;
    class Texture;
}

// Settings for your own draws (Device::draw).
namespace tgx::gl {
    // How consecutive vertices are assembled into primitives.
    enum class Primitive : std::int32_t {
        triangles,
        triangle_strip,
        lines,
        line_strip,
        points,
    };

    // Depth test against the stored depth.
    enum class Depth : std::int32_t {
        // Later draws cover earlier ones.
        none,
        less,
        less_equal,
    };

    // Which triangles are skipped. The front is counter-clockwise on screen.
    enum class Cull : std::int32_t {
        none,
        back,
        front,
    };

    enum class Fill : std::int32_t {
        solid,
        // Only triangle edges.
        wireframe,
    };

    // Fixed-function settings, given in full per draw so nothing leaks into
    // the next. The Device changes only what differs.
    struct RenderState {
        Blend blend{Blend::none};
        Depth depth{Depth::none};
        // Off for transparent geometry drawn after opaque. No effect without a
        // depth test: GL then stores no depth.
        bool depth_write{true};
        Cull cull{Cull::none};
        Fill fill{Fill::solid};

        [[nodiscard]] constexpr auto operator==(const RenderState &) const noexcept -> bool = default;
    };

    // In framebuffer pixels from the top-left corner, like the rest of tgx,
    // not from the bottom-left like glViewport.
    struct Viewport {
        int x{0};
        int y{0};
        int width{0};
        int height{0};

        [[nodiscard]] constexpr auto operator==(const Viewport &) const noexcept -> bool = default;
    };

    // Given per draw, like RenderState; nothing carries over.
    struct DrawParams {
        // For count: from first to the end of the buffer.
        static constexpr std::size_t all = std::numeric_limits<std::size_t>::max();

        // nullptr: the window.
        const RenderTarget *target{nullptr};
        // First vertex, or first index.
        std::size_t first{0};
        // Vertices, or indices with an index buffer. 0 draws nothing.
        std::size_t count{all};
        Primitive primitive{Primitive::triangles};
        RenderState state{};
        // Empty (default): the whole target. A part is for split screens and
        // the like.
        std::optional<Viewport> viewport{};
        // By slot (gl::TextureSlot). Every slot a sampler reads needs a
        // texture. Empty slots are left unchanged.
        //
        //     device.draw(shader, quad, {.textures = {&texture}});
        std::array<const Texture *, max_texture_slots> textures{};
    };
}
