#pragma once

#include "tgx/core/handle.hpp"

#include "tgx/gl/shader.hpp"
#include "tgx/gl/vertex_array.hpp"

#include <array>
#include <cstdint>

namespace tgx::detail {
    struct CanvasShaderBindings {
        // 0: the built-in shader.
        GlId program{0};
        std::int32_t u_projection{-1};
        // -1: the shader does not sample a texture.
        std::int32_t u_texture{-1};
    };

    // Shared by the batch's VAO and the custom shader checks.
    [[nodiscard]] auto canvas_vertex_layout() noexcept -> std::array<gl::VertexAttribute, 3>;

    // Checks the active inputs, required mat4 u_projection and optional
    // sampler2D u_texture, then returns their bindings. Checks follow
    // TGX_ENABLE_ASSERTS. nullptr selects the built-in shader.
    [[nodiscard]] auto canvas_shader_bindings(const gl::Shader *shader) noexcept -> CanvasShaderBindings;
}
