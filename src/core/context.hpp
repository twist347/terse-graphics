#pragma once

#include "tgx/core/color.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/image.hpp"
#include "tgx/core/math.hpp"
#include "tgx/core/render_target.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/vertex_array.hpp"

#include "core/clock_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace tgx::detail {
    class Batch;

    // The two sizes of what is drawn into. The only place that tells the
    // window from a render target: everything else measures through this.
    struct Surface {
        // What viewports are measured in.
        Size pixels{};
        // What a canvas over all of it spans: the window's screen
        // coordinates, which differ from its pixels on a scaling display.
        Size units{};

        // All of it, in pixels.
        [[nodiscard]] constexpr auto viewport() const noexcept -> gl::Viewport {
            return {0, 0, pixels.width, pixels.height};
        }
    };

    [[nodiscard]] auto surface_of(const Target &target) noexcept -> Surface;

    // One clear, by ids: what Device::clear and Canvas::clear come down to.
    // What is left empty stays as it is.
    struct ClearCall {
        Target target{};
        std::optional<Color> color{};
        std::optional<float> depth{};
    };

    // One draw, by ids: what Device::draw and the batch both come down to.
    struct DrawCall {
        Target target{};
        GlId program{0};
        GlId vertex_array{0};
        // Empty to draw straight from the vertices.
        std::optional<gl::IndexType> index_type{};
        // Vertices, or indices with an index type.
        std::size_t first{0};
        std::size_t count{0};
        gl::Primitive primitive{gl::Primitive::triangles};
        gl::RenderState state{};
        // From the top-left, as tgx counts; turned for GL as it is drawn,
        // against the target as it is then, the way GL itself takes a
        // viewport.
        gl::Viewport viewport{};
        // By slot; 0 leaves the slot as it is.
        std::array<GlId, gl::max_texture_slots> textures{};
    };

    // The one GL context, while a Device owns it: what GL is set to now, so
    // that setting what is already set costs nothing, and what the Canvas has
    // collected. There is one window, so there is one of these; the Device is
    // only the handle that sets it up and tears it down. Main thread only.
    struct Context {
        gl::RenderState state{};
        Color clear_color{0, 0, 0, 0};
        float clear_depth{1.f};
        // As glViewport took it: from the bottom-left, unlike every other
        // gl::Viewport in tgx.
        gl::Viewport gl_viewport{};
        // What draws and clears go into; 0 for the window.
        GlId framebuffer{0};
        // What glUseProgram last made current; 0 for none.
        GlId program{0};
        // What glBindVertexArray last bound; 0 for none.
        GlId vertex_array{0};
        // The texture bound in each slot, 0 for none, and the slot
        // glActiveTexture last selected.
        std::array<GlId, gl::max_texture_slots> textures{};
        std::uint32_t active_slot{0};

        // The frames presented, timed; Clock shows it.
        FrameClock clock;

        // Null while it is being made and while it goes: its own shader and
        // texture reach the flush hooks then. unique_ptr is null before it
        // deletes, which optional does not promise.
        std::unique_ptr<Batch> batch;

        // Starts afresh on a context just made current: forgets everything
        // and puts GL where a fresh Context says it is, as a GL context can
        // outlive a Device and keep what the last one set. Leaves no batch.
        auto reset() noexcept -> void;

        // Draws what the batch has collected, if anything.
        auto flush() noexcept -> void;

        // Draws the batch first. Clears all of the target.
        auto clear(const ClearCall &call) noexcept -> void;

        // Draws the batch first, then the call; checks nothing, the callers
        // do what they need of that.
        auto draw(const DrawCall &call) noexcept -> void;

        // The call alone, without drawing the batch first: for the batch
        // drawing itself.
        auto submit(const DrawCall &call) noexcept -> void;

        // Draws the batch first, then the target's pixels back on the CPU,
        // rows top to bottom as in any Image (GL hands them bottom up).
        [[nodiscard]] auto read(const Target &target) -> Image;

        // Makes the framebuffer the one drawn into unless it already is.
        auto bind_framebuffer(GlId framebuffer) noexcept -> void;

        // For a framebuffer about to be deleted: GL goes back to the window's
        // if it was bound, and its id may come back for the next one created.
        auto forget_framebuffer(GlId framebuffer) noexcept -> void;

        // Makes the program current unless it already is.
        auto use_program(GlId program) noexcept -> void;

        // For a program about to be deleted: GL may hand its id to the next
        // program created, which must not pass for the current one.
        auto forget_program(GlId program) noexcept -> void;

        // Binds the vertex array unless it already is.
        auto bind_vertex_array(GlId vertex_array) noexcept -> void;

        // For a vertex array about to be deleted: GL unbinds it, and its id
        // may come back for the next one created.
        auto forget_vertex_array(GlId vertex_array) noexcept -> void;

        // Binds the texture to the slot (texture unit) unless it already is
        // there, leaving that slot active. The slot is below
        // gl::max_texture_slots.
        auto bind_texture(std::uint32_t slot, GlId texture) noexcept -> void;

        // For a texture about to be deleted: GL unbinds it from every slot, and
        // its id may come back for the next texture created.
        auto forget_texture(GlId texture) noexcept -> void;
    };

    [[nodiscard]] auto context() noexcept -> Context &;

    // For a texture or a program about to change or go: if the batch is to be
    // drawn with it, that happens now, while it is still as it was when the
    // shapes were added. So the batch never reads anything later than the
    // calls that filled it.
    auto flush_texture_use(GlId texture) noexcept -> void;

    auto flush_shader_use(GlId program) noexcept -> void;

    // For a render target about to go: shapes waiting to be drawn into it are.
    // Whichever of its framebuffer and its texture goes first draws them
    // (flush_texture_use covers the texture).
    auto flush_target_use(GlId framebuffer) noexcept -> void;
}
