#pragma once

#include "tgx/core/version.hpp"

// The OpenGL version tgx requests and the minimum it accepts. Macros, so
// shader sources can be joined with them; the constants below are for C++.
#define TGX_GL_VERSION_MAJOR 3
#define TGX_GL_VERSION_MINOR 3

// The #version line, as a string literal to put before a shader source:
//
//     constexpr const char *source = TGX_GLSL_VERSION R"(
//         ...
//     )";
//
// Since GL 3.3, the GLSL version is the GL version plus a 0.
#define TGX_GLSL_VERSION \
    "#version " TGX_DETAIL_STR(TGX_GL_VERSION_MAJOR) TGX_DETAIL_STR(TGX_GL_VERSION_MINOR) "0 core\n"

namespace tgx::gl {
    inline constexpr int version_major = TGX_GL_VERSION_MAJOR;
    inline constexpr int version_minor = TGX_GL_VERSION_MINOR;
}
