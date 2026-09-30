#pragma once

#include "tgx/gl/handle.h"

// GL state the Device tracks for the rest of the library, so that binding what
// is already bound costs nothing. Main thread only, like the Device.
namespace tgx::detail {
    // Makes the program current unless it already is.
    auto use_program(gl::GlId program) noexcept -> void;

    // For a program about to be deleted: GL may hand its id to the next program
    // created, which must not pass for the current one.
    auto forget_program(gl::GlId program) noexcept -> void;
}
