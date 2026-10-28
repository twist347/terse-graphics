#pragma once

#include "tgx/gl/shader.hpp"
#include "tgx/gl/vertex_array.hpp"

namespace tgx::gl::detail {
    // Asserts that every shader input has an attribute of the same kind,
    // integer or float. GL reports neither: a missing attribute reads a
    // constant (usually 0, 0, 0, 1), a kind mismatch reads garbage. Fewer
    // components are fine: GL fills in 0, 0, 1. For Device::draw.
    auto check_vertex_inputs(const Shader &shader, const VertexArray &vertices) noexcept -> void;
}
