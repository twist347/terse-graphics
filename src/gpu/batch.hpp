#pragma once

#include "tgx/core/blend.hpp"
#include "tgx/core/color.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/math.hpp"
#include "tgx/core/render_target.hpp"

#include "tgx/gl/draw.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tgx::detail {
    // The layout the built-in shader reads, and a custom one must too
    // (Canvas::set_shader).
    struct BatchVertex {
        Vec2 position{};
        Vec2 uv{};
        Color color{};
    };

    // What the waiting vertices are drawn with; a different state draws them
    // first. Holds ids, so a moved Texture or Shader stays valid.
    struct BatchState {
        Target target{};
        // 0: the built-in texture (the default font and white for shapes).
        GlId texture{0};
        Blend blend{Blend::alpha};
        // 0: the built-in shader.
        GlId program{0};
        // A custom shader's locations, -1 if absent. Unused by the built-in
        // shader.
        std::int32_t u_projection{-1};
        std::int32_t u_texture{-1};
        // To clip space; goes to u_projection.
        Mat4 transform{};
        // In pixels from the top-left.
        gl::Viewport viewport{};
        // In pixels from the top-left. None: not clipped.
        std::optional<gl::Viewport> clip{};

        [[nodiscard]] auto operator==(const BatchState &) const noexcept -> bool = default;
    };

    // CPU geometry waiting to be drawn together. Context decides when to
    // draw it before a state change, a capacity boundary or another operation.
    class Batch {
    public:
        Batch();
        Batch(const Batch &) = delete;
        auto operator=(const Batch &) -> Batch & = delete;
        Batch(Batch &&) noexcept = default;
        auto operator=(Batch &&) noexcept -> Batch & = default;

        [[nodiscard]] auto can_append(
            const BatchState &state,
            std::size_t vertex_count,
            std::size_t index_count
        ) const noexcept -> bool;

        // After can_append succeeds. Returns the first new vertex's index.
        // Push exactly the requested counts before the next operation.
        [[nodiscard]] auto start(const BatchState &state) noexcept -> std::uint16_t;

        auto push_vertex(const BatchVertex &vertex) noexcept -> void { vertices_.push_back(vertex); }
        auto push_index(std::uint16_t index) noexcept -> void { indices_.push_back(index); }

        [[nodiscard]] auto empty() const noexcept -> bool { return indices_.empty(); }
        [[nodiscard]] auto state() const noexcept -> const BatchState & { return state_; }
        [[nodiscard]] auto vertices() const noexcept -> std::span<const BatchVertex> { return vertices_; }
        [[nodiscard]] auto indices() const noexcept -> std::span<const std::uint16_t> { return indices_; }

        auto clear() noexcept -> void;

        // Whether waiting vertices read the texture, draw into it (a render
        // target's texture), or use the program/framebuffer.
        [[nodiscard]] auto uses_texture(GlId texture) const noexcept -> bool;
        [[nodiscard]] auto uses_shader(GlId program) const noexcept -> bool;
        [[nodiscard]] auto uses_target(GlId framebuffer) const noexcept -> bool;

    private:
        std::vector<BatchVertex> vertices_{};
        std::vector<std::uint16_t> indices_{};
        BatchState state_{};
    };

    // Per draw. Indices are 16-bit, so under 65536 vertices. A circle uses
    // about 3 indices per vertex, the most of any shape.
    inline constexpr std::size_t batch_max_vertices = 16384;
    inline constexpr std::size_t batch_max_indices = batch_max_vertices * 3;
}
