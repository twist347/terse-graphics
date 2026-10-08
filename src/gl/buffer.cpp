#include "tgx/gl/buffer.hpp"

#include "tgx/core/assert.hpp"

#include "core/gl_error.hpp"

#include <glad/gl.h>

#include <utility>

namespace {
    // Buffers are bound here only to be edited. COPY_WRITE is not read by draws
    // and is not VAO state, so binding to it cannot disturb anything; binding
    // to ELEMENT_ARRAY would swap the index buffer of whatever VAO is bound.
    constexpr GLenum edit_target = GL_COPY_WRITE_BUFFER;

    // Only a hint in 3.3: nothing stops a rewrite, the assert in update() does.
    [[nodiscard]] constexpr auto to_gl(tgx::gl::BufferAccess access) noexcept -> GLenum {
        switch (access) {
            case tgx::gl::BufferAccess::immutable: return GL_STATIC_DRAW;
            case tgx::gl::BufferAccess::dynamic: return GL_DYNAMIC_DRAW;
        }
        return GL_STATIC_DRAW;
    }
}

namespace tgx::gl {
    auto detail::delete_buffer(GlId id) noexcept -> void {
        glDeleteBuffers(1, &id);
    }

    auto detail::create_buffer(
        std::size_t byte_size,
        const void *data,
        BufferAccess access
    ) noexcept -> Result<GlId> {
        // GL rejects empty storage, and sizes travel as a signed GLsizeiptr.
        TGX_ASSERT(byte_size > 0);
        TGX_ASSERT(std::in_range<GLsizeiptr>(byte_size));

        tgx::detail::drain_gl_errors();

        GLuint id = 0;
        glGenBuffers(1, &id);
        glBindBuffer(edit_target, id);
        glBufferData(edit_target, static_cast<GLsizeiptr>(byte_size), data, to_gl(access));

        // Any error leaves the buffer without storage, so none is survivable.
        if (const GLenum err = glGetError(); err != GL_NO_ERROR) {
            detail::delete_buffer(id);
            return std::unexpected{tgx::detail::to_error(err)};
        }
        return id;
    }

    auto detail::update_buffer(GlId id, std::size_t byte_offset, std::span<const std::byte> data) noexcept -> void {
        if (data.empty()) {
            return;
        }

        glBindBuffer(edit_target, id);
        glBufferSubData(
            edit_target,
            static_cast<GLintptr>(byte_offset),
            static_cast<GLsizeiptr>(data.size()),
            data.data()
        );
    }
}
