#include "tgx/gl/shader.h"

#include "tgx/assert.h"

#include <glad/gl.h>

#include <utility>

namespace {
    auto delete_shader(tgx::gl::GlId id) noexcept -> void {
        glDeleteShader(id);
    }

    // Stage objects are only needed until the program is linked.
    using Stage = tgx::gl::Handle<delete_shader>;

    // Shaders and programs keep their logs behind different, same-shaped calls.
    enum class LogOf {
        shader,
        program,
    };

    // Appends "<label>:\n<log>" when the driver has anything to say. The length
    // GL reports includes the terminating null.
    auto append_log(std::string *out_log, std::string_view label, GLuint object, LogOf kind) -> void {
        if (out_log == nullptr) {
            return;
        }

        GLint length = 0;
        if (kind == LogOf::program) {
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
        if (kind == LogOf::program) {
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

    // Returns an empty handle if the stage failed to compile.
    [[nodiscard]] auto compile(
        GLenum stage,
        std::string_view label,
        std::string_view source,
        std::string *out_log
    ) -> Stage {
        TGX_ASSERT(std::in_range<GLint>(source.size()));

        // Owned from the start: append_log allocates and may throw.
        Stage shader{glCreateShader(stage)};

        const GLchar *text = source.data();
        const auto length = static_cast<GLint>(source.size());
        glShaderSource(shader.get(), 1, &text, &length);
        glCompileShader(shader.get());

        append_log(out_log, label, shader.get(), LogOf::shader);

        GLint ok = GL_FALSE;
        glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &ok);
        if (ok != GL_TRUE) {
            return {};
        }
        return shader;
    }
}

namespace tgx::gl {
    auto detail::delete_program(GlId id) noexcept -> void {
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
        const Stage vs = compile(GL_VERTEX_SHADER, "vertex", vertex, out_log);
        const Stage fs = compile(GL_FRAGMENT_SHADER, "fragment", fragment, out_log);
        if (!vs || !fs) {
            return std::unexpected{Error::compile};
        }

        Handle<detail::delete_program> program{glCreateProgram()};
        glAttachShader(program.get(), vs.get());
        glAttachShader(program.get(), fs.get());
        glLinkProgram(program.get());

        // The program keeps what it linked; the stages are deleted on return.
        glDetachShader(program.get(), vs.get());
        glDetachShader(program.get(), fs.get());

        append_log(out_log, "link", program.get(), LogOf::program);

        GLint ok = GL_FALSE;
        glGetProgramiv(program.get(), GL_LINK_STATUS, &ok);
        if (ok != GL_TRUE) {
            return std::unexpected{Error::link};
        }
        return Shader{std::move(program)};
    }

    auto Shader::id() const noexcept -> GlId {
        TGX_ASSERT(m_handle);

        return m_handle.get();
    }
}
