#include "tgx/gl/shader.h"

#include <climits>

#include <glad/gl.h>

#include "tgx/assert.h"

namespace {
    // Appends "<label>:\n<log>" when the driver has anything to say. The length
    // GL reports includes the terminating null.
    void append_log(std::string *out_log, std::string_view label, GLuint object, bool is_program) {
        if (out_log == nullptr) {
            return;
        }

        GLint length = 0;
        if (is_program) {
            glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
        } else {
            glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
        }
        if (length <= 1) {
            return;
        }

        out_log->append(label);
        out_log->append(":\n");

        // GL writes at most length bytes counting its own terminator: make room
        // for that, then shrink to what it actually produced.
        const std::size_t at = out_log->size();
        out_log->resize(at + static_cast<std::size_t>(length));
        GLsizei written = 0;
        if (is_program) {
            glGetProgramInfoLog(object, length, &written, out_log->data() + at);
        } else {
            glGetShaderInfoLog(object, length, &written, out_log->data() + at);
        }
        out_log->resize(at + static_cast<std::size_t>(written));

        // Non-empty for sure: "<label>:\n" is already in there.
        if (out_log->back() != '\n') {
            out_log->push_back('\n');
        }
    }

    // Returns the shader object, or 0 if it failed to compile.
    [[nodiscard]] auto compile(
        GLenum stage,
        std::string_view label,
        std::string_view source,
        std::string *out_log
    ) -> GLuint {
        TGX_ASSERT(source.size() <= static_cast<std::size_t>(INT_MAX));

        const GLuint shader = glCreateShader(stage);

        const GLchar *text = source.data();
        const auto length = static_cast<GLint>(source.size());
        glShaderSource(shader, 1, &text, &length);
        glCompileShader(shader);

        append_log(out_log, label, shader, false);

        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (ok != GL_TRUE) {
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
}

namespace tgx::gl {
    void detail::delete_program(GlId id) noexcept {
        glDeleteProgram(id);
    }

    auto Shader::from_source(
        Device &,
        std::string_view vertex,
        std::string_view fragment,
        std::string *out_log
    ) -> Result<Shader> {
        if (out_log != nullptr) {
            out_log->clear();
        }

        // Both stages are compiled even if the first fails, so one run reports
        // every error.
        const GLuint vs = compile(GL_VERTEX_SHADER, "vertex", vertex, out_log);
        const GLuint fs = compile(GL_FRAGMENT_SHADER, "fragment", fragment, out_log);
        if (vs == 0 || fs == 0) {
            glDeleteShader(vs);
            glDeleteShader(fs);
            return std::unexpected{Error::compile};
        }

        const GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        // The program keeps what it linked; the stage objects are no longer needed.
        glDetachShader(program, vs);
        glDetachShader(program, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        append_log(out_log, "link", program, true);

        GLint ok = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (ok != GL_TRUE) {
            glDeleteProgram(program);
            return std::unexpected{Error::link};
        }
        return Shader{program};
    }

    auto Shader::id() const noexcept -> GlId {
        TGX_ASSERT(m_handle);

        return m_handle.get();
    }
}
