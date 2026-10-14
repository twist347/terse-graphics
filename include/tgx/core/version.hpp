#pragma once

// The only place the version is written; CMakeLists.txt reads it. Before
// 1.0 a breaking change bumps minor, as the package's compatibility check
// assumes.

// Bumped by a breaking change, from 1.0 on.
#define TGX_VERSION_MAJOR 0

// Bumped by an addition, and before 1.0 by a breaking change.
#define TGX_VERSION_MINOR 1

// Bumped by a fix.
#define TGX_VERSION_PATCH 0

// One comparable number. minor and patch must be below 1000.
#define TGX_VERSION_NUM(major, minor, patch) ((major) * 1000000 + (minor) * 1000 + (patch))

// This version as a TGX_VERSION_NUM.
#define TGX_VERSION TGX_VERSION_NUM(TGX_VERSION_MAJOR, TGX_VERSION_MINOR, TGX_VERSION_PATCH)

// For #if:
//
//     #if TGX_VERSION_AT_LEAST(0, 2, 0)
#define TGX_VERSION_AT_LEAST(major, minor, patch) (TGX_VERSION >= TGX_VERSION_NUM(major, minor, patch))

// Two steps to expand the argument: "0", not "TGX_VERSION_MAJOR".
#define TGX_DETAIL_STR_(x) #x
#define TGX_DETAIL_STR(x) TGX_DETAIL_STR_(x)

// A string literal, "0.1.0".
#define TGX_VERSION_STRING \
    TGX_DETAIL_STR(TGX_VERSION_MAJOR) "." TGX_DETAIL_STR(TGX_VERSION_MINOR) "." TGX_DETAIL_STR(TGX_VERSION_PATCH)
