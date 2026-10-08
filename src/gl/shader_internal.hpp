#pragma once

#include "tgx/core/math.hpp"

#include <cstdint>

namespace tgx::gl::detail {
    // Sets a mat4 uniform of the program in use, straight to GL: for
    // Shader::set and for the batch, which must not go through Shader::set
    // as that would come back to flush it.
    auto upload_mat4(std::int32_t location, const Mat4 &value) noexcept -> void;
}
