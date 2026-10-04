#pragma once

#include "tgx/error.h"

#include <glad/gl.h>

// The GL error check around giving a resource its storage, the one place a
// driver failure is reported rather than asserted.
namespace tgx::detail {
    // Drains errors left over from earlier calls, so a check right after an
    // allocation is about it only. GL keeps one flag per kind of error, so a
    // few calls empty it; the bound is for a lost context, on which some
    // drivers report an error on every call.
    inline auto drain_gl_errors() noexcept -> void {
        for (int i = 0; i < 16 && glGetError() != GL_NO_ERROR; ++i) {
        }
    }

    // What an error raised by an allocation means: the driver out of room, or
    // failing otherwise. Caller mistakes are asserted before, so none is left.
    [[nodiscard]] constexpr auto to_error(GLenum err) noexcept -> Error {
        return err == GL_OUT_OF_MEMORY ? Error::out_of_mem : Error::platform;
    }
}
