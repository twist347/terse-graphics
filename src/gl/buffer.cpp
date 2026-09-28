#include "tgx/gl/buffer.h"

#include <cstdint>

#include <glad/gl.h>

#include "tgx/assert.h"

namespace {
    [[nodiscard]] auto to_gl_flags(tgx::gl::BufferAccess access) noexcept -> GLbitfield {
        switch (access) {
            case tgx::gl::BufferAccess::immutable: return 0;
            case tgx::gl::BufferAccess::dynamic: return GL_DYNAMIC_STORAGE_BIT;
        }
        return 0;
    }

    [[nodiscard]] auto make(
        std::size_t size,
        const void *data,
        tgx::gl::BufferAccess access
    ) noexcept -> tgx::Result<GLuint> {
        // GL rejects empty storage, and sizes travel as a signed GLsizeiptr.
        TGX_ASSERT(size > 0);
        TGX_ASSERT(size <= static_cast<std::size_t>(PTRDIFF_MAX));

        GLuint id = 0;
        glCreateBuffers(1, &id);

        // Drain errors left over from earlier calls, so the check below is
        // about this allocation only.
        while (glGetError() != GL_NO_ERROR) {
        }

        glNamedBufferStorage(id, static_cast<GLsizeiptr>(size), data, to_gl_flags(access));

        if (glGetError() == GL_OUT_OF_MEMORY) {
            glDeleteBuffers(1, &id);
            return std::unexpected{tgx::Error::out_of_mem};
        }
        return id;
    }
}

namespace tgx::gl {
    auto detail::delete_buffer(GlId id) noexcept -> void {
        glDeleteBuffers(1, &id);
    }

    auto Buffer::create(Device &, std::size_t size, BufferAccess access) noexcept -> Result<Buffer> {
        return make(size, nullptr, access).transform([&](GLuint id) {
            return Buffer{id, size, access};
        });
    }

    auto Buffer::create(Device &, std::span<const std::byte> data, BufferAccess access) noexcept -> Result<Buffer> {
        return make(data.size(), data.data(), access).transform([&](GLuint id) {
            return Buffer{id, data.size(), access};
        });
    }

    auto Buffer::update(std::size_t byte_offset, std::span<const std::byte> data) noexcept -> void {
        TGX_ASSERT(m_handle);
        TGX_ASSERT_MSG(m_access == BufferAccess::dynamic, "updating an immutable buffer");
        TGX_ASSERT_MSG(
            byte_offset <= m_size && data.size() <= m_size - byte_offset,
            "update of {} bytes at {} overruns a {}-byte buffer",
            data.size(),
            byte_offset,
            m_size
        );

        if (data.empty()) {
            return;
        }

        glNamedBufferSubData(
            m_handle.get(),
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
