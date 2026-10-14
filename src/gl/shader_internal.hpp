#pragma once

#include "tgx/core/math.hpp"

#include <cstdint>

namespace tgx::gl::detail {
    // Sets a mat4 uniform of the current program directly in GL. The batch
    // uses it because Shader::set would flush the batch itself.
    auto upload_mat4(std::int32_t location, const Mat4 &value) noexcept -> void;
}
