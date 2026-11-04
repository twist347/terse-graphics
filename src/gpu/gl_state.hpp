#pragma once

#include "tgx/core/color.hpp"
#include "tgx/core/device.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/image.hpp"
#include "tgx/core/render_target.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/vertex_array.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace tgx::detail {
    // One clear, with GL ids. Empty fields are not cleared.
    struct ClearCall {
        Target target{};
        std::optional<Color> color{};
        std::optional<float> depth{};
    };

    // One draw, with GL ids, from Device::draw or the batch.
    struct DrawCall {
        Target target{};
        GlId program{0};
        GlId vertex_array{0};
        // Empty: no indices.
        std::optional<gl::IndexType> index_type{};
        // In vertices, or in indices when index_type is set.
        std::size_t first{0};
        std::size_t count{0};
        gl::Primitive primitive{gl::Primitive::triangles};
        gl::RenderState state{};
        // From the top-left. Flipped for GL at draw time, with the target's
        // size at that moment, as GL itself applies a viewport.
        gl::Viewport viewport{};
        // Pixels outside it are not drawn. From the top-left, as viewport.
        // The scissor test is on only for the draw, as raw GL expects it off.
        std::optional<gl::Viewport> scissor{};
        // 0 leaves the slot unchanged.
        std::array<GlId, gl::max_texture_slots> textures{};
    };

    // Executes immediately and caches the GL state it sets. Knows nothing
    // about waiting geometry, frame timing or resource ownership.
    class GlState {
    public:
        // For a GL context just made current. Sets GL to the tracked defaults
        // and queries the window's depth attachment.
        auto reset() noexcept -> void;

        [[nodiscard]] auto has_depth(const Target &target) const noexcept -> bool {
            return target.framebuffer == 0 ? window_depth_bits_ > 0 : target.depth;
        }

        auto clear(const ClearCall &call) noexcept -> void;
        auto draw(const DrawCall &call) noexcept -> void;

        auto count_canvas_upload(std::size_t vertices, std::size_t indices, std::size_t bytes) noexcept -> void {
            work_.canvas_vertices += vertices;
            work_.canvas_indices += indices;
            work_.canvas_upload_bytes += bytes;
        }

        // Rows top to bottom (GL gives them bottom up).
        [[nodiscard]] auto read(const Target &target) -> Image;

        // Unless already bound/current. Texture binding leaves the slot
        // active; slot must be below gl::max_texture_slots.
        auto bind_framebuffer(GlId next) noexcept -> void;
        auto use_program(GlId next) noexcept -> void;
        auto bind_vertex_array(GlId next) noexcept -> void;
        auto bind_texture(std::uint32_t slot, GlId texture) noexcept -> void;

        // Call before deletion: GL unbinds the object and may reuse its id.
        auto forget_framebuffer(GlId id) noexcept -> void;
        auto forget_program(GlId id) noexcept -> void;
        auto forget_vertex_array(GlId id) noexcept -> void;
        auto forget_texture(GlId texture) noexcept -> void;

        // Actual GL work since the previous take. Context adds canvas_draws.
        [[nodiscard]] auto take_stats() noexcept -> FrameStats { return std::exchange(work_, {}); }

    private:
        int window_depth_bits_{0};
        gl::RenderState state_{};
        Color clear_color_{0, 0, 0, 0};
        float clear_depth_{1.f};
        // As passed to glViewport: from the bottom-left, unlike every other
        // gl::Viewport in tgx.
        gl::Viewport gl_viewport_{};
        GlId framebuffer_{0};
        GlId program_{0};
        GlId vertex_array_{0};
        std::array<GlId, gl::max_texture_slots> textures_{};
        std::uint32_t active_slot_{0};
        FrameStats work_{};
    };
}
