#pragma once

#include "tgx/gl/shader.h"
#include "tgx/gl/vertex_array.h"

namespace tgx::gl::detail {
    // Asserts that every input the shader reads has an attribute of the same
    // kind, integer or float. GL feeds an input with no attribute a constant
    // (usually 0, 0, 0, 1), and reads integers into a float input or the other
    // way round as garbage; neither is reported. A different component count
    // is fine: GL pads the missing ones with 0, 0, 1. For the asserts in
    // Device::draw alone.
    auto check_vertex_inputs(const Shader &shader, const VertexArray &vertices) noexcept -> void;
}
