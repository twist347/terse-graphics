#pragma once

#include "tgx/core/error.h"

#include <glad/gl.h>

// The GL error check around giving a resource its storage, the one place a
// driver failure is reported rather than asserted.
namespace tgx::detail {
    // GL keeps one flag per kind of error, so a few calls empty them; the
    // bound is for a lost context, on which some drivers report an error on
    // every call.
    inline constexpr int max_drained_errors = 16;

    // Drains errors left over from earlier calls, so a check right after an
    // allocation is about it only.
    inline auto drain_gl_errors() noexcept -> void {
        for (int i = 0; i < max_drained_errors && glGetError() != GL_NO_ERROR; ++i) {
        }
    }

    // What an error raised by an allocation means: the driver out of room, or
    // failing otherwise. Caller mistakes are asserted before, so none is left.
    [[nodiscard]] constexpr auto to_error(GLenum err) noexcept -> Error {
        return err == GL_OUT_OF_MEMORY ? Error::out_of_memory : Error::platform;
    }
}
