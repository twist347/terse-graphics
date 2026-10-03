#include "tgx/gl/shader.h"

#include "tgx/assert.h"

#include "context.h"

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
    using tgx::gl::detail::ComponentKind;
    using tgx::gl::detail::ShaderSampler;
    using tgx::gl::detail::ShaderUniform;
    using tgx::gl::detail::VertexInput;

    auto delete_shader(tgx::GlId id) noexcept -> void {
        glDeleteShader(id);
    }

    // Stage objects are only needed until the program is linked.
    using Stage = tgx::Handle<delete_shader>;

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

    [[nodiscard]] constexpr auto to_component_kind(GLenum type) noexcept -> ComponentKind {
        switch (type) {
            case GL_INT:
            case GL_INT_VEC2:
            case GL_INT_VEC3:
            case GL_INT_VEC4: return ComponentKind::sint;
            case GL_UNSIGNED_INT:
            case GL_UNSIGNED_INT_VEC2:
            case GL_UNSIGNED_INT_VEC3:
            case GL_UNSIGNED_INT_VEC4: return ComponentKind::uint;
            default: return ComponentKind::floating;
        }
    }

    // A matCxR input takes one location per column.
    [[nodiscard]] constexpr auto slots_of(GLenum type) noexcept -> std::uint32_t {
        switch (type) {
            case GL_FLOAT_MAT2:
            case GL_FLOAT_MAT2x3:
            case GL_FLOAT_MAT2x4: return 2;
            case GL_FLOAT_MAT3:
            case GL_FLOAT_MAT3x2:
            case GL_FLOAT_MAT3x4: return 3;
            case GL_FLOAT_MAT4:
            case GL_FLOAT_MAT4x2:
            case GL_FLOAT_MAT4x3: return 4;
            default: return 1;
        }
    }

    [[nodiscard]] auto collect_uniforms(GLuint program) -> std::vector<ShaderUniform> {
        GLint count = 0;
        GLint max_length = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
        glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_length);

        std::vector<ShaderUniform> uniforms;
        // Room for the longest name and GL's terminator, which the location
        // query below relies on.
        std::string name(static_cast<std::size_t>(max_length), '\0');
        for (GLint i = 0; i < count; ++i) {
            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveUniform(program, static_cast<GLuint>(i), max_length, &length, &size, &type, name.data());

            // Members of uniform blocks have none: they are set through the
            // block's buffer, not one by one.
            const GLint location = glGetUniformLocation(program, name.c_str());
            if (location < 0) {
                continue;
            }

            // Arrays are reported by their first element; the location found
            // is the array's.
            std::string_view base{name.data(), static_cast<std::size_t>(length)};
            if (base.ends_with("[0]")) {
                base.remove_suffix(3);
            }
            uniforms.push_back({std::string{base}, location, type, size});
        }
        return uniforms;
    }

    [[nodiscard]] auto collect_samplers(std::span<const ShaderUniform> uniforms) -> std::vector<ShaderSampler> {
        std::vector<ShaderSampler> samplers;
        for (const auto &uniform : uniforms) {
            if (uniform.gl_type == GL_SAMPLER_2D) {
                samplers.push_back({uniform.name, uniform.location});
            }
        }
        return samplers;
    }

    [[nodiscard]] auto collect_vertex_inputs(GLuint program) -> std::vector<VertexInput> {
        GLint count = 0;
        GLint max_length = 0;
        glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);
        glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_length);

        std::vector<VertexInput> inputs;
        std::string name(static_cast<std::size_t>(max_length), '\0');
        for (GLint i = 0; i < count; ++i) {
            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveAttrib(program, static_cast<GLuint>(i), max_length, &length, &size, &type, name.data());

            // Built-ins such as gl_VertexID come from GL, not from a buffer.
            const GLint location = glGetAttribLocation(program, name.c_str());
            if (location < 0) {
                continue;
            }

            inputs.push_back({
                std::string{name.data(), static_cast<std::size_t>(length)},
                static_cast<std::uint32_t>(location),
                slots_of(type) * static_cast<std::uint32_t>(size),
                to_component_kind(type),
            });
        }
        return inputs;
    }

    // GLSL spelling of a uniform type, for assert messages.
    [[nodiscard]] constexpr auto glsl_name(GLenum type) noexcept -> const char * {
        switch (type) {
            case GL_FLOAT: return "float";
            case GL_FLOAT_VEC2: return "vec2";
            case GL_FLOAT_VEC3: return "vec3";
            case GL_FLOAT_VEC4: return "vec4";
            case GL_INT: return "int";
            case GL_INT_VEC2: return "ivec2";
            case GL_INT_VEC3: return "ivec3";
            case GL_INT_VEC4: return "ivec4";
            case GL_UNSIGNED_INT: return "uint";
            case GL_UNSIGNED_INT_VEC2: return "uvec2";
            case GL_UNSIGNED_INT_VEC3: return "uvec3";
            case GL_UNSIGNED_INT_VEC4: return "uvec4";
            case GL_BOOL: return "bool";
            case GL_BOOL_VEC2: return "bvec2";
            case GL_BOOL_VEC3: return "bvec3";
            case GL_BOOL_VEC4: return "bvec4";
            case GL_FLOAT_MAT2: return "mat2";
            case GL_FLOAT_MAT3: return "mat3";
            case GL_FLOAT_MAT4: return "mat4";
            case GL_FLOAT_MAT2x3: return "mat2x3";
            case GL_FLOAT_MAT2x4: return "mat2x4";
            case GL_FLOAT_MAT3x2: return "mat3x2";
            case GL_FLOAT_MAT3x4: return "mat3x4";
            case GL_FLOAT_MAT4x2: return "mat4x2";
            case GL_FLOAT_MAT4x3: return "mat4x3";
            case GL_SAMPLER_2D: return "sampler2D";
            default: return "another type";
        }
    }

    // The uniform's location, or -1 if there is no such uniform.
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
            glsl_name(uniform->gl_type),
            glsl_name(accepted.front())
        );
        TGX_ASSERT_MSG(
            uniform->count == 1,
            "uniform '{}' is an array of {}; arrays are not supported yet",
            name,
            uniform->count
        );
        return uniform->location;
    }

    // How a value of T reaches a uniform: the GLSL types it may be set on, and
    // the glUniform* call that writes it to the current program. One per type
    // UniformValue admits.
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
            // Column-major already, so no transpose.
            const auto floats = std::bit_cast<std::array<float, 16>>(value);
            glUniformMatrix4fv(location, 1, GL_FALSE, floats.data());
        }
    };
}

namespace tgx::gl {
    auto detail::delete_program(GlId id) noexcept -> void {
        // Shapes added before keep the program they were added with.
        tgx::detail::flush_shader_use(id);
        tgx::detail::context().forget_program(id);
        glDeleteProgram(id);
    }

    auto detail::vertex_inputs(const Shader &shader) noexcept -> std::span<const VertexInput> {
        return shader.m_inputs;
    }

    auto detail::samplers(const Shader &shader) noexcept -> std::span<const ShaderSampler> {
        return shader.m_samplers;
    }

    auto Shader::from_source(
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

        // Read before the handle moves into the Shader. The vertex inputs only
        // feed an assert in Device::draw, so builds without asserts skip them.
        auto uniforms = collect_uniforms(program.get());
        auto samplers = collect_samplers(uniforms);
        std::vector<VertexInput> inputs;
        if constexpr (TGX_ENABLE_ASSERTS != 0) {
            inputs = collect_vertex_inputs(program.get());
        }
        return Shader{std::move(program), std::move(uniforms), std::move(inputs), std::move(samplers)};
    }

    template<UniformValue T>
    auto Shader::uniform(std::string_view name) const noexcept -> Uniform<T> {
        return Uniform<T>{id(), find_location(m_uniforms, name, UniformTraits<T>::types)};
    }

    template<UniformValue T>
    auto Shader::set(Uniform<T> uniform, const std::type_identity_t<T> &value) noexcept -> void {
        TGX_ASSERT_MSG(uniform.m_program == id(), "setting a uniform not looked up from this shader");

        if (uniform.m_location < 0) {
            return;
        }

        // Shapes added before keep the values they were added with.
        tgx::detail::flush_shader_use(id());
        if constexpr (std::same_as<T, TextureSlot>) {
            TGX_ASSERT_MSG(
                value.index < max_texture_slots,
                "texture slot {} is out of range, there are {}",
                value.index, max_texture_slots
            );
            const auto sampler = std::ranges::find(m_samplers, uniform.m_location, &detail::ShaderSampler::location);
            if (sampler != m_samplers.end()) {
                sampler->slot = value.index;
            }
        }

        tgx::detail::context().use_program(id());
        UniformTraits<T>::upload(uniform.m_location, value);
    }

    // Every type UniformValue admits, so the templates need no definition in
    // the header.
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

    auto Shader::id() const noexcept -> GlId {
        return m_handle.get();
    }
}
