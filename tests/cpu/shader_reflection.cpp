// Sampler collection uses reflected metadata, without a GL context.

#include "gpu/gl/shader_reflection.hpp"

#include <doctest/doctest.h>
#include <glad/gl.h>

#include <array>
#include <cstddef>
#include <string_view>

TEST_CASE("sampler reflection keeps unsupported texture types and ignores ordinary uniforms") {
    using tgx::gl::detail::ShaderUniform;
    const std::array uniforms{
        ShaderUniform{"u_projection", 0, GL_FLOAT_MAT4, 1},
        ShaderUniform{"u_cube", 1, GL_SAMPLER_CUBE, 1},
        ShaderUniform{"u_color", 2, GL_FLOAT_VEC4, 1},
        ShaderUniform{"u_texture", 3, GL_SAMPLER_2D, 1},
        ShaderUniform{"u_shadow", 4, GL_SAMPLER_2D_SHADOW, 1},
        ShaderUniform{"u_int", 5, GL_INT_SAMPLER_2D, 1},
        ShaderUniform{"u_uint", 6, GL_UNSIGNED_INT_SAMPLER_2D, 1},
        ShaderUniform{"u_samples", 7, GL_SAMPLER_2D_MULTISAMPLE, 1},
        ShaderUniform{"u_layers", 8, GL_SAMPLER_2D_ARRAY, 1},
        ShaderUniform{"u_rect", 9, GL_INT_SAMPLER_2D_RECT, 1},
        ShaderUniform{"u_buffer", 10, GL_UNSIGNED_INT_SAMPLER_BUFFER, 1},
        ShaderUniform{"u_1d", 11, GL_SAMPLER_1D_ARRAY_SHADOW, 1},
        ShaderUniform{"u_sampler_named_float", 12, GL_FLOAT, 1},
    };
    constexpr std::array<std::size_t, 10> expected{1, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    const auto samplers = tgx::gl::detail::collect_samplers(uniforms);
    REQUIRE(samplers.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto &uniform = uniforms[expected[i]];
        CAPTURE(uniform.name);
        CHECK(samplers[i].name == uniform.name);
        CHECK(samplers[i].location == uniform.location);
        CHECK(samplers[i].gl_type == uniform.gl_type);
    }
    CHECK(std::string_view{tgx::gl::detail::glsl_name(GL_SAMPLER_CUBE)} == "samplerCube");
    CHECK(std::string_view{tgx::gl::detail::glsl_name(GL_INT_SAMPLER_2D)} == "isampler2D");
    CHECK(std::string_view{tgx::gl::detail::glsl_name(GL_SAMPLER_2D_MULTISAMPLE)} == "sampler2DMS");
}
