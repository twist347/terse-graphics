#pragma once

#include "tgx/core/error.hpp"

#include "core/log_internal.hpp"

#include <glad/gl.h>

// The GL error check around giving a resource its storage, the one place a
// driver failure is reported rather than asserted.
namespace tgx::detail {
    // GL keeps one flag per kind of error, so a few calls empty them; the
    // bound is for a lost context, on which some drivers report an error on
    // every call.
    inline constexpr int max_drained_errors = 16;

    // Drains errors left over from earlier calls, so a check right after an
    // allocation is about it only. They are logged rather than lost: tgx's
    // own calls are asserted, so one is most likely raw GL's, and without a
    // debug context this is its only trace.
    inline auto drain_gl_errors() noexcept -> void {
        for (int i = 0; i < max_drained_errors; ++i) {
            const GLenum err = glGetError();
            if (err == GL_NO_ERROR) {
                break;
            }
            log_warn("GL error 0x{:04x} left by an earlier call", err);
        }
    }

    // What an error raised by an allocation means: the driver out of room, or
    // failing otherwise. Caller mistakes are asserted before, so none is left.
    [[nodiscard]] constexpr auto to_error(GLenum err) noexcept -> Error {
        return err == GL_OUT_OF_MEMORY ? Error::out_of_memory : Error::platform;
    }
}
