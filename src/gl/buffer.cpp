#include "tgx/gl/buffer.h"

#include "tgx/assert.h"

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

    [[nodiscard]] auto make(
        std::size_t size,
        const void *data,
        tgx::gl::BufferAccess access
    ) noexcept -> tgx::Result<GLuint> {
        TGX_ASSERT_MSG(tgx::gl::detail::context_alive(), "creating a Buffer before the Device");
        // GL rejects empty storage, and sizes travel as a signed GLsizeiptr.
        TGX_ASSERT(size > 0);
        TGX_ASSERT(std::in_range<GLsizeiptr>(size));

        // Drain errors left over from earlier calls, so the check below is
        // about this allocation only. GL keeps one flag per kind of error, so a
        // few calls empty it; the bound is for a lost context, on which some
        // drivers report an error on every call.
        for (int i = 0; i < 16 && glGetError() != GL_NO_ERROR; ++i) {
        }

        GLuint id = 0;
        glGenBuffers(1, &id);
        glBindBuffer(edit_target, id);
        glBufferData(edit_target, static_cast<GLsizeiptr>(size), data, to_gl(access));

        // Any error leaves the buffer without storage, so none is survivable.
        // Caller mistakes are asserted above; what is left is the driver.
        if (const GLenum err = glGetError(); err != GL_NO_ERROR) {
            glDeleteBuffers(1, &id);
            return std::unexpected{err == GL_OUT_OF_MEMORY ? tgx::Error::out_of_mem : tgx::Error::platform};
        }
        return id;
    }
}

namespace tgx::gl {
    auto detail::delete_buffer(GlId id) noexcept -> void {
        glDeleteBuffers(1, &id);
    }

    auto Buffer::create(
        std::size_t size,
        BufferAccess access
    ) noexcept -> Result<Buffer> {
        TGX_ASSERT_MSG(access == BufferAccess::dynamic, "an immutable buffer without data can never be filled");

        return make(size, nullptr, access).transform([&](GLuint id) {
            return Buffer{id, size, access};
        });
    }

    auto Buffer::create_bytes(
        std::span<const std::byte> data,
        BufferAccess access
    ) noexcept -> Result<Buffer> {
        return make(data.size(), data.data(), access).transform([&](GLuint id) {
            return Buffer{id, data.size(), access};
        });
    }

    auto Buffer::update_bytes(std::size_t byte_offset, std::span<const std::byte> data) noexcept -> void {
        TGX_ASSERT(m_handle);
        TGX_ASSERT_MSG(m_access == BufferAccess::dynamic, "updating an immutable buffer");
        TGX_ASSERT_MSG(
            byte_offset <= m_size && data.size() <= m_size - byte_offset,
            "update of {} bytes at {} overruns a {}-byte buffer",
            data.size(), byte_offset, m_size
        );

        if (data.empty()) {
            return;
        }

        glBindBuffer(edit_target, m_handle.get());
        glBufferSubData(
            edit_target,
            static_cast<GLintptr>(byte_offset),
            static_cast<GLsizeiptr>(data.size()),
            data.data()
        );
    }

    auto Buffer::id() const noexcept -> GlId {
        TGX_ASSERT(m_handle);

        return m_handle.get();
    }

    auto Buffer::size() const noexcept -> std::size_t {
        TGX_ASSERT(m_handle);

        return m_size;
    }

    auto Buffer::access() const noexcept -> BufferAccess {
        TGX_ASSERT(m_handle);

        return m_access;
    }
}
