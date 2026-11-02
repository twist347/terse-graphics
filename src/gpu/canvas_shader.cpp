#include "gpu/canvas_shader.hpp"

#include "tgx/core/assert.hpp"
#include "tgx/core/math.hpp"

#include "tgx/gl/texture_slot.hpp"

#include "gpu/batch.hpp"
#include "gpu/gl/shader_reflection.hpp"
#include "gpu/gl/vertex_array_internal.hpp"

#include <glad/gl.h>

#include <array>

namespace tgx::detail {
    auto canvas_vertex_layout() noexcept -> std::array<gl::VertexAttribute, 3> {
        return {
            gl::VertexAttribute::of(0, &BatchVertex::position),
            gl::VertexAttribute::of(1, &BatchVertex::uv),
            gl::VertexAttribute::of(2, &BatchVertex::color),
        };
    }

    auto canvas_shader_bindings(const gl::Shader *shader) noexcept -> CanvasShaderBindings {
        if (!shader) {
            return {};
        }
        if constexpr (TGX_ENABLE_ASSERTS != 0) {
            const auto layout = canvas_vertex_layout();
            gl::detail::check_vertex_inputs(*shader, layout);
        }

        CanvasShaderBindings bindings{
            .program = shader->id(),
            .u_projection = gl::detail::location(shader->uniform<Mat4>("u_projection")),
        };

        const auto samplers = gl::detail::samplers(*shader);
        for (const auto &sampler: samplers) {
            TGX_ASSERT_MSG(
                sampler.gl_type == GL_SAMPLER_2D,
                "sampler '{}' is {}; a canvas shader supports only sampler2D u_texture",
                sampler.name, gl::detail::glsl_name(sampler.gl_type)
            );
            TGX_ASSERT_MSG(
                sampler.name == "u_texture",
                "sampler '{}': a canvas shader gets no texture but u_texture",
                sampler.name
            );
        }
        if (!samplers.empty()) {
            bindings.u_texture = gl::detail::location(shader->uniform<gl::TextureSlot>("u_texture"));
        }
        return bindings;
    }
}
