#pragma once

// The version of tgx: the one place it is written, CMakeLists.txt reads it
// from here. Before 1.0 the minor number is bumped by a change that breaks a
// caller, as the installed package's compatibility check assumes.

// Bumped by a change that breaks a caller, from 1.0 on.
#define TGX_VERSION_MAJOR 0

// Bumped by an addition; before 1.0, by a break too.
#define TGX_VERSION_MINOR 1

// Bumped by a fix.
#define TGX_VERSION_PATCH 0

// The version as one comparable number; minor and patch below 1000.
#define TGX_VERSION_NUM(major, minor, patch) ((major) * 1000000 + (minor) * 1000 + (patch))

// This tgx as TGX_VERSION_NUM.
#define TGX_VERSION TGX_VERSION_NUM(TGX_VERSION_MAJOR, TGX_VERSION_MINOR, TGX_VERSION_PATCH)

// Whether this tgx is at least the given version, for #if:
//
//     #if TGX_VERSION_AT_LEAST(0, 2, 0)
#define TGX_VERSION_AT_LEAST(major, minor, patch) (TGX_VERSION >= TGX_VERSION_NUM(major, minor, patch))

// The two steps make the argument arrive expanded: "0", not "TGX_VERSION_MAJOR".
#define TGX_DETAIL_STR_(x) #x
#define TGX_DETAIL_STR(x) TGX_DETAIL_STR_(x)

// The version as a string literal, "0.1.0".
#define TGX_VERSION_STRING \
    TGX_DETAIL_STR(TGX_VERSION_MAJOR) "." TGX_DETAIL_STR(TGX_VERSION_MINOR) "." TGX_DETAIL_STR(TGX_VERSION_PATCH)
