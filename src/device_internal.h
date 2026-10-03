#pragma once

#include "tgx/gl/handle.h"

#include <cstdint>

namespace tgx::gl {
    class Device;

    namespace detail {
        class Batch;

        // The Device's internals, for the parts of tgx built on it.
        struct DeviceAccess {
            [[nodiscard]] static auto batch(Device &device) noexcept -> Batch &;
            // nullptr while the Device is making its batch or dropping it.
            [[nodiscard]] static auto batch_if_any(Device &device) noexcept -> Batch *;
        };
    }
}

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

    // The Device that exists, for the Canvas, which draws through it without
    // holding it: moving the Device (into an App, out of a Result) cannot
    // leave the Canvas pointing at the old place.
    [[nodiscard]] auto device() noexcept -> gl::Device &;

    // For a texture about to move or go: if the batch is to be drawn with it,
    // that happens now, while the Texture is still there to draw from.
    auto flush_texture_use(gl::GlId texture) noexcept -> void;
}
