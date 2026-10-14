#pragma once

#include "tgx/core/error.hpp"
#include "tgx/core/handle.hpp"

#include "core/log_internal.hpp"

#include <glad/gl.h>

#include <expected>

// GL error checks around allocating storage. The only place where a driver
// failure is reported as an error instead of asserted.
namespace tgx::detail {
    // GL keeps one flag per kind of error, so a few calls clear them all. The
    // limit is for a lost context, where some drivers error on every call.
    inline constexpr int max_drained_errors = 16;

    // Call before an allocation, so the check after it sees only its errors.
    // Logs them: tgx's own calls are asserted, so they likely come from raw
    // GL, and without a debug context this is their only trace.
    inline auto drain_gl_errors() noexcept -> void {
        for (int i = 0; i < max_drained_errors; ++i) {
            const GLenum err = glGetError();
            if (err == GL_NO_ERROR) {
                break;
            }
            log_warn("GL error 0x{:04x} left by an earlier call", err);
        }
    }

    // For an allocation error. Caller mistakes are asserted before, so it is
    // never one of those.
    [[nodiscard]] constexpr auto to_error(GLenum err) noexcept -> Error {
        return err == GL_OUT_OF_MEMORY ? Error::out_of_memory : Error::platform;
    }

    // Call after allocating an object's storage. On an error, deletes the
    // object as a Handle with the same Delete would.
    template<auto Delete>
    [[nodiscard]] auto checked_allocation(GlId id) noexcept -> Result<GlId> {
        if (const GLenum err = glGetError(); err != GL_NO_ERROR) {
            Delete(id);
            return std::unexpected{to_error(err)};
        }
        return id;
    }
}
