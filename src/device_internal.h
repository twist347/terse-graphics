#pragma once

#include "tgx/gl/handle.h"

#include <cstdint>

// GL state the Device tracks for the rest of the library, so that binding what
// is already bound costs nothing. Main thread only, like the Device.
namespace tgx::detail {
    // Makes the program current unless it already is.
    auto use_program(gl::GlId program) noexcept -> void;

    // For a program about to be deleted: GL may hand its id to the next program
    // created, which must not pass for the current one.
    auto forget_program(gl::GlId program) noexcept -> void;

    // Binds the texture to the slot (texture unit) unless it already is there,
    // leaving that slot active. The slot is below gl::max_texture_slots.
    auto bind_texture(std::uint32_t slot, gl::GlId texture) noexcept -> void;

    // For a texture about to be deleted: GL unbinds it from every slot, and its
    // id may come back for the next texture created.
    auto forget_texture(gl::GlId texture) noexcept -> void;
}
