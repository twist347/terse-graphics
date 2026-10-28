#pragma once

#include "tgx/core/handle.hpp"

#include "tgx/gl/shader.hpp"

#include <cstdint>
#include <span>
#include <vector>

// What a linked program takes, read back from GL.
namespace tgx::gl::detail {
    [[nodiscard]] auto collect_uniforms(GlId program) -> std::vector<ShaderUniform>;

    [[nodiscard]] auto collect_samplers(std::span<const ShaderUniform> uniforms) -> std::vector<ShaderSampler>;

    // Only for the vertex input assert in Device::draw.
    [[nodiscard]] auto collect_vertex_inputs(GlId program) -> std::vector<VertexInput>;

    // For assert messages: "vec4" for GL_FLOAT_VEC4.
    [[nodiscard]] auto glsl_name(std::uint32_t type) noexcept -> const char *;
}
