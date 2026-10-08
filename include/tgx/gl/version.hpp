#pragma once

#include "tgx/core/version.hpp"

// The OpenGL version tgx is written against: the context it requests and the
// minimum it accepts. Macros, so that shader sources can be glued to them at
// compile time; the constants below carry the same values for C++ code.
#define TGX_GL_VERSION_MAJOR 3
#define TGX_GL_VERSION_MINOR 3

// The #version line for TGX_GL_VERSION, as a string literal to glue in front
// of a shader source:
//
//     constexpr const char *source = TGX_GLSL_VERSION R"(
//         ...
//     )";
//
// From GL 3.3 on, the GLSL version is the GL version followed by a 0.
#define TGX_GLSL_VERSION \
    "#version " TGX_DETAIL_STR(TGX_GL_VERSION_MAJOR) TGX_DETAIL_STR(TGX_GL_VERSION_MINOR) "0 core\n"

namespace tgx::gl {
    inline constexpr int version_major = TGX_GL_VERSION_MAJOR;
    inline constexpr int version_minor = TGX_GL_VERSION_MINOR;
}
