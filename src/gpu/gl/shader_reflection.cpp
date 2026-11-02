#include "gpu/gl/shader_reflection.hpp"

#include <glad/gl.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using tgx::gl::detail::ComponentKind;

    // Every sampler type in GL 3.3. Also used for diagnostics, so collecting
    // a type and naming it cannot get out of sync.
    [[nodiscard]] constexpr auto sampler_name(GLenum type) noexcept -> const char * {
        switch (type) {
            case GL_SAMPLER_1D: return "sampler1D";
            case GL_SAMPLER_2D: return "sampler2D";
            case GL_SAMPLER_3D: return "sampler3D";
            case GL_SAMPLER_CUBE: return "samplerCube";
            case GL_SAMPLER_1D_SHADOW: return "sampler1DShadow";
            case GL_SAMPLER_2D_SHADOW: return "sampler2DShadow";
            case GL_SAMPLER_1D_ARRAY: return "sampler1DArray";
            case GL_SAMPLER_2D_ARRAY: return "sampler2DArray";
            case GL_SAMPLER_1D_ARRAY_SHADOW: return "sampler1DArrayShadow";
            case GL_SAMPLER_2D_ARRAY_SHADOW: return "sampler2DArrayShadow";
            case GL_SAMPLER_CUBE_SHADOW: return "samplerCubeShadow";
            case GL_SAMPLER_BUFFER: return "samplerBuffer";
            case GL_SAMPLER_2D_RECT: return "sampler2DRect";
            case GL_SAMPLER_2D_RECT_SHADOW: return "sampler2DRectShadow";
            case GL_SAMPLER_2D_MULTISAMPLE: return "sampler2DMS";
            case GL_SAMPLER_2D_MULTISAMPLE_ARRAY: return "sampler2DMSArray";
            case GL_INT_SAMPLER_1D: return "isampler1D";
            case GL_INT_SAMPLER_2D: return "isampler2D";
            case GL_INT_SAMPLER_3D: return "isampler3D";
            case GL_INT_SAMPLER_CUBE: return "isamplerCube";
            case GL_INT_SAMPLER_1D_ARRAY: return "isampler1DArray";
            case GL_INT_SAMPLER_2D_ARRAY: return "isampler2DArray";
            case GL_INT_SAMPLER_BUFFER: return "isamplerBuffer";
            case GL_INT_SAMPLER_2D_RECT: return "isampler2DRect";
            case GL_INT_SAMPLER_2D_MULTISAMPLE: return "isampler2DMS";
            case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: return "isampler2DMSArray";
            case GL_UNSIGNED_INT_SAMPLER_1D: return "usampler1D";
            case GL_UNSIGNED_INT_SAMPLER_2D: return "usampler2D";
            case GL_UNSIGNED_INT_SAMPLER_3D: return "usampler3D";
            case GL_UNSIGNED_INT_SAMPLER_CUBE: return "usamplerCube";
            case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY: return "usampler1DArray";
            case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY: return "usampler2DArray";
            case GL_UNSIGNED_INT_SAMPLER_BUFFER: return "usamplerBuffer";
            case GL_UNSIGNED_INT_SAMPLER_2D_RECT: return "usampler2DRect";
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE: return "usampler2DMS";
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: return "usampler2DMSArray";
            default: return nullptr;
        }
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

    // A matCxR input uses one location per column.
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

    // Calls each(name, location, type, size) for every active uniform or
    // vertex input that has a location: uniform block members and built-ins
    // such as gl_VertexID have none. get_active and get_location are GL's
    // calls for one kind (glGetActiveUniform, glGetUniformLocation) or the
    // other (glGetActiveAttrib, glGetAttribLocation); they take the same
    // arguments.
    template<typename Each>
    auto for_each_active(
        tgx::GlId program,
        GLenum count_query,
        GLenum length_query,
        PFNGLGETACTIVEUNIFORMPROC get_active,
        PFNGLGETUNIFORMLOCATIONPROC get_location,
        Each each
    ) -> void {
        GLint count = 0;
        GLint max_length = 0;
        glGetProgramiv(program, count_query, &count);
        glGetProgramiv(program, length_query, &max_length);

        // Longest name plus GL's terminator, which the location query needs.
        std::string name(static_cast<std::size_t>(max_length), '\0');
        for (GLint i = 0; i < count; ++i) {
            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;
            get_active(program, static_cast<GLuint>(i), max_length, &length, &size, &type, name.data());
            if (const GLint location = get_location(program, name.c_str()); location >= 0) {
                each(std::string_view{name.data(), static_cast<std::size_t>(length)}, location, type, size);
            }
        }
    }
}

namespace tgx::gl {
    auto detail::collect_uniforms(GlId program) -> std::vector<ShaderUniform> {
        std::vector<ShaderUniform> uniforms{};
        for_each_active(
            program, GL_ACTIVE_UNIFORMS, GL_ACTIVE_UNIFORM_MAX_LENGTH, glGetActiveUniform, glGetUniformLocation,
            [&](std::string_view name, GLint location, GLenum type, GLint size) {
                // An array is reported by its first element, at the array's
                // location.
                if (name.ends_with("[0]")) {
                    name.remove_suffix(3);
                }
                uniforms.push_back({std::string{name}, location, type, size});
            }
        );
        return uniforms;
    }

    auto detail::collect_samplers(std::span<const ShaderUniform> uniforms) -> std::vector<ShaderSampler> {
        std::vector<ShaderSampler> samplers{};
        for (const auto &uniform: uniforms) {
            if (sampler_name(uniform.gl_type)) {
                samplers.push_back({uniform.name, uniform.location, uniform.gl_type});
            }
        }
        return samplers;
    }

    auto detail::collect_vertex_inputs(GlId program) -> std::vector<VertexInput> {
        std::vector<VertexInput> inputs{};
        for_each_active(
            program, GL_ACTIVE_ATTRIBUTES, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, glGetActiveAttrib, glGetAttribLocation,
            [&](std::string_view name, GLint location, GLenum type, GLint size) {
                inputs.push_back({
                    std::string{name},
                    static_cast<std::uint32_t>(location),
                    slots_of(type) * static_cast<std::uint32_t>(size),
                    to_component_kind(type),
                });
            }
        );
        return inputs;
    }

    auto detail::glsl_name(std::uint32_t type) noexcept -> const char * {
        if (const char *name = sampler_name(type)) {
            return name;
        }
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
            default: return "another type";
        }
    }
}
