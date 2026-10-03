#pragma once

#include "tgx/color.h"

#include "tgx/gl/device.h"
#include "tgx/gl/handle.h"
#include "tgx/gl/texture_slot.h"
#include "tgx/gl/vertex_array.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

struct GLFWwindow;

namespace tgx::gl::detail {
    class Batch;

    // One draw, by ids: what Device::draw and the batch both come down to.
    struct DrawCall {
        GlId program{0};
        GlId vertex_array{0};
        // Empty to draw straight from the vertices.
        std::optional<IndexType> index_type{};
        // Vertices, or indices with an index type.
        std::size_t first{0};
        std::size_t count{0};
        Primitive primitive{Primitive::triangles};
        RenderState state{};
        // By slot; 0 leaves the slot as it is.
        std::array<GlId, max_texture_slots> textures{};
    };

    // The one GL context, while a Device owns it: what GL is set to now, so
    // that setting what is already set costs nothing, and what the Canvas has
    // collected. There is one window, so there is one of these; the Device is
    // only the handle that sets it up and tears it down. Main thread only.
    struct Context {
        // The window frames are presented to.
        GLFWwindow *window{nullptr};

        RenderState state{};
        Color clear_color{0, 0, 0, 0};
        float clear_depth{1.f};
        std::int32_t clear_stencil{0};
        std::array<int, 4> viewport{};
        // What glUseProgram last made current; 0 for none.
        GlId program{0};
        // The texture bound in each slot, 0 for none, and the slot
        // glActiveTexture last selected.
        std::array<GlId, max_texture_slots> textures{};
        std::uint32_t active_slot{0};

        // Null while it is being made and while it goes: its own shader and
        // texture reach the flush hooks then. unique_ptr is null before it
        // deletes, which optional does not promise.
        std::unique_ptr<Batch> batch;

        // Draws what the batch has collected, if anything.
        auto flush() noexcept -> void;

        // Both draw the batch first.
        auto clear(const ClearParams &params) noexcept -> void;
        auto set_viewport(int x, int y, int width, int height) noexcept -> void;

        // Neither draws the batch first nor checks anything: the callers do
        // what they need of both.
        auto draw(const DrawCall &call) noexcept -> void;

        // Makes the program current unless it already is.
        auto use_program(GlId program) noexcept -> void;

        // For a program about to be deleted: GL may hand its id to the next
        // program created, which must not pass for the current one.
        auto forget_program(GlId program) noexcept -> void;

        // Binds the texture to the slot (texture unit) unless it already is
        // there, leaving that slot active. The slot is below max_texture_slots.
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
}
