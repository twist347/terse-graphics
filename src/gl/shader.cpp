#include "tgx/gl/shader.hpp"

#include "tgx/core/assert.hpp"

#include "core/context.hpp"
#include "gl/shader_internal.hpp"
#include "gl/shader_reflection.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
    using tgx::gl::detail::ShaderUniform;
    using tgx::gl::detail::VertexInput;

    auto delete_shader(tgx::GlId id) noexcept -> void {
        glDeleteShader(id);
    }

    // Stage objects are only needed until the program is linked.
    using Stage = tgx::detail::Handle<delete_shader>;

    // Appends "<label>:\n<log>" when the log is not empty. GL's length
    // includes the terminating null. get_length and get_log are GL's calls
    // for a stage (glGetShader*) or a program (glGetProgram*); the two take
    // the same arguments.
    auto append_log(
        std::string *out_log,
        std::string_view label,
        GLuint object,
        PFNGLGETSHADERIVPROC get_length,
        PFNGLGETSHADERINFOLOGPROC get_log
    ) -> void {
        if (!out_log) {
            return;
        }

        GLint length = 0;
        get_length(object, GL_INFO_LOG_LENGTH, &length);
        if (length <= 1) {
            return;
        }

        out_log->append(label);
        out_log->append(":\n");

        // Room for length bytes with the terminator, then shrink to what GL
        // wrote.
        const std::size_t at = out_log->size();
        out_log->resize(at + static_cast<std::size_t>(length));
        GLsizei written = 0;
        get_log(object, length, &written, out_log->data() + at);
        out_log->resize(at + static_cast<std::size_t>(written));

        // Never empty here: "<label>:\n" is already in it.
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

        // Owned right away: append_log may throw.
        Stage shader{glCreateShader(stage)};

        const GLchar *text = source.data();
        const auto length = static_cast<GLint>(source.size());
        glShaderSource(shader.get(), 1, &text, &length);
        glCompileShader(shader.get());

        append_log(out_log, label, shader.get(), glGetShaderiv, glGetShaderInfoLog);

        GLint ok = GL_FALSE;
        glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &ok);
        if (ok != GL_TRUE) {
            return {};
        }
        return shader;
    }

    // -1 if there is no such uniform.
    [[nodiscard]] auto find_location(
        std::span<const ShaderUniform> uniforms,
        std::string_view name,
        std::span<const GLenum> accepted
    ) noexcept -> GLint {
        const auto uniform = std::ranges::find(uniforms, name, &ShaderUniform::name);
        TGX_ASSERT_MSG(
            uniform != uniforms.end(),
            "no active uniform '{}': misspelled, or unused and optimized out by the compiler",
            name
        );
        if (uniform == uniforms.end()) {
            return -1;
        }

        TGX_ASSERT_MSG(
            std::ranges::contains(accepted, uniform->gl_type),
            "uniform '{}' is {}, not {}",
            name,
            tgx::gl::detail::glsl_name(uniform->gl_type),
            tgx::gl::detail::glsl_name(accepted.front())
        );
        TGX_ASSERT_MSG(
            uniform->count == 1,
            "uniform '{}' is an array of {}; arrays are not supported yet",
            name,
            uniform->count
        );
        return uniform->location;
    }

    // The GLSL types T can set, and the glUniform* call that writes it to the
    // current program. One per UniformValue type.
    template<typename T>
    struct UniformTraits;

    template<>
    struct UniformTraits<float> {
        static constexpr std::array<GLenum, 1> types{GL_FLOAT};

        static auto upload(GLint location, float value) noexcept -> void {
            glUniform1f(location, value);
        }
    };

    template<>
    struct UniformTraits<std::int32_t> {
        static constexpr std::array<GLenum, 2> types{GL_INT, GL_BOOL};

        static auto upload(GLint location, std::int32_t value) noexcept -> void {
            glUniform1i(location, value);
        }
    };

    template<>
    struct UniformTraits<std::uint32_t> {
        static constexpr std::array<GLenum, 2> types{GL_UNSIGNED_INT, GL_BOOL};

        static auto upload(GLint location, std::uint32_t value) noexcept -> void {
            glUniform1ui(location, value);
        }
    };

    template<>
    struct UniformTraits<tgx::Vec2> {
        static constexpr std::array<GLenum, 1> types{GL_FLOAT_VEC2};

        static auto upload(GLint location, tgx::Vec2 value) noexcept -> void {
            glUniform2f(location, value.x, value.y);
        }
    };

    template<>
    struct UniformTraits<tgx::Vec3> {
        static constexpr std::array<GLenum, 1> types{GL_FLOAT_VEC3};

        static auto upload(GLint location, tgx::Vec3 value) noexcept -> void {
            glUniform3f(location, value.x, value.y, value.z);
        }
    };

    template<>
    struct UniformTraits<tgx::Vec4> {
        static constexpr std::array<GLenum, 1> types{GL_FLOAT_VEC4};

        static auto upload(GLint location, tgx::Vec4 value) noexcept -> void {
            glUniform4f(location, value.x, value.y, value.z, value.w);
        }
    };

    template<>
    struct UniformTraits<tgx::Color> {
        static constexpr std::array<GLenum, 1> types{GL_FLOAT_VEC4};

        static auto upload(GLint location, tgx::Color value) noexcept -> void {
            UniformTraits<tgx::Vec4>::upload(location, tgx::to_vec4(value));
        }
    };

    template<>
    struct UniformTraits<tgx::gl::TextureSlot> {
        static constexpr std::array<GLenum, 1> types{GL_SAMPLER_2D};

        static auto upload(GLint location, tgx::gl::TextureSlot value) noexcept -> void {
            glUniform1i(location, static_cast<GLint>(value.index));
        }
    };

    template<>
    struct UniformTraits<tgx::Mat4> {
        static constexpr std::array<GLenum, 1> types{GL_FLOAT_MAT4};

        static auto upload(GLint location, const tgx::Mat4 &value) noexcept -> void {
            tgx::gl::detail::upload_mat4(location, value);
        }
    };
}

namespace tgx::gl {
    auto detail::delete_program(GlId id) noexcept -> void {
        // Draws the batched shapes while their program still exists.
        tgx::detail::flush_shader_use(id);
        tgx::detail::context().forget_program(id);
        glDeleteProgram(id);
    }

    auto detail::vertex_inputs(const Shader &shader) noexcept -> std::span<const VertexInput> {
        return shader.inputs_;
    }

    auto detail::samplers(const Shader &shader) noexcept -> std::span<const ShaderSampler> {
        return shader.samplers_;
    }

    auto Shader::from_source(
        std::string_view vertex,
        std::string_view fragment,
        std::string *out_log
    ) -> Result<Shader> {
        if (out_log) {
            out_log->clear();
        }

        // Compiles both stages even if one fails, to report all errors.
        const Stage vs = compile(GL_VERTEX_SHADER, "vertex", vertex, out_log);
        const Stage fs = compile(GL_FRAGMENT_SHADER, "fragment", fragment, out_log);
        if (!vs || !fs) {
            return std::unexpected{Error::compile};
        }

        tgx::detail::Handle<detail::delete_program> program{glCreateProgram()};
        glAttachShader(program.get(), vs.get());
        glAttachShader(program.get(), fs.get());
        glLinkProgram(program.get());

        // The linked program does not need the stages.
        glDetachShader(program.get(), vs.get());
        glDetachShader(program.get(), fs.get());

        append_log(out_log, "link", program.get(), glGetProgramiv, glGetProgramInfoLog);

        GLint ok = GL_FALSE;
        glGetProgramiv(program.get(), GL_LINK_STATUS, &ok);
        if (ok != GL_TRUE) {
            return std::unexpected{Error::link};
        }

        // Read before the handle moves. Vertex inputs are only for an assert.
        auto uniforms = detail::collect_uniforms(program.get());
        auto samplers = detail::collect_samplers(uniforms);
        std::vector<VertexInput> inputs{};
        if constexpr (TGX_ENABLE_ASSERTS != 0) {
            inputs = detail::collect_vertex_inputs(program.get());
        }
        return Shader{std::move(program), std::move(uniforms), std::move(inputs), std::move(samplers)};
    }

    template<UniformValue T>
    auto Shader::uniform(std::string_view name) const noexcept -> Uniform<T> {
        return Uniform<T>{id(), find_location(uniforms_, name, UniformTraits<T>::types)};
    }

    template<UniformValue T>
    auto Shader::set(Uniform<T> uniform, const std::type_identity_t<T> &value) noexcept -> void {
        TGX_ASSERT_MSG(uniform.program_ == id(), "setting a uniform not looked up from this shader");

        if (uniform.location_ < 0) {
            return;
        }

        // Draws the batched shapes first, so they keep the old values.
        tgx::detail::flush_shader_use(id());
        if constexpr (std::same_as<T, TextureSlot>) {
            TGX_ASSERT_MSG(
                value.index < max_texture_slots,
                "texture slot {} is out of range, there are {}",
                value.index, max_texture_slots
            );
        }

        tgx::detail::context().use_program(id());
        UniformTraits<T>::upload(uniform.location_, value);
    }

    // Every UniformValue type, so the header needs no definitions.
    template auto Shader::uniform<float>(std::string_view) const noexcept -> Uniform<float>;
    template auto Shader::uniform<std::int32_t>(std::string_view) const noexcept -> Uniform<std::int32_t>;
    template auto Shader::uniform<std::uint32_t>(std::string_view) const noexcept -> Uniform<std::uint32_t>;
    template auto Shader::uniform<Vec2>(std::string_view) const noexcept -> Uniform<Vec2>;
    template auto Shader::uniform<Vec3>(std::string_view) const noexcept -> Uniform<Vec3>;
    template auto Shader::uniform<Vec4>(std::string_view) const noexcept -> Uniform<Vec4>;
    template auto Shader::uniform<Mat4>(std::string_view) const noexcept -> Uniform<Mat4>;
    template auto Shader::uniform<Color>(std::string_view) const noexcept -> Uniform<Color>;
    template auto Shader::uniform<TextureSlot>(std::string_view) const noexcept -> Uniform<TextureSlot>;

    template auto Shader::set<float>(Uniform<float>, const float &) noexcept -> void;
    template auto Shader::set<std::int32_t>(Uniform<std::int32_t>, const std::int32_t &) noexcept -> void;
    template auto Shader::set<std::uint32_t>(Uniform<std::uint32_t>, const std::uint32_t &) noexcept -> void;
    template auto Shader::set<Vec2>(Uniform<Vec2>, const Vec2 &) noexcept -> void;
    template auto Shader::set<Vec3>(Uniform<Vec3>, const Vec3 &) noexcept -> void;
    template auto Shader::set<Vec4>(Uniform<Vec4>, const Vec4 &) noexcept -> void;
    template auto Shader::set<Mat4>(Uniform<Mat4>, const Mat4 &) noexcept -> void;
    template auto Shader::set<Color>(Uniform<Color>, const Color &) noexcept -> void;
    template auto Shader::set<TextureSlot>(Uniform<TextureSlot>, const TextureSlot &) noexcept -> void;

    auto detail::upload_mat4(std::int32_t location, const Mat4 &value) noexcept -> void {
        // Mat4 is column-major: no transpose.
        const auto floats = std::bit_cast<std::array<float, 16>>(value);
        glUniformMatrix4fv(location, 1, GL_FALSE, floats.data());
    }
}
