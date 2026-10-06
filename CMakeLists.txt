cmake_minimum_required(VERSION 3.25)

project(terse-graphics LANGUAGES C CXX)

# On when tgx is the project being built, off when another project adds it
# with add_subdirectory or FetchContent.
option(TGX_BUILD_EXAMPLES "Build tgx examples" ${PROJECT_IS_TOP_LEVEL})
option(TGX_BUILD_TESTS "Build tgx tests" ${PROJECT_IS_TOP_LEVEL})
option(TGX_WARNINGS_AS_ERRORS "Treat warnings in tgx, its examples and tests as errors" OFF)
set(TGX_ASSERTS "" CACHE STRING "tgx asserts: ON, OFF, or empty to follow the build type")
set_property(CACHE TGX_ASSERTS PROPERTY STRINGS "" ON OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Standard C++ for every target here, examples and tests included: what
# builds with GCC must not lean on its extensions to build with MSVC.
set(CMAKE_CXX_EXTENSIONS OFF)

# For tgx's own code only; the copies in thirdparty/ build as they come.
# MSVC: /permissive- for standard name lookup, /utf-8 so sources read the same
# whatever the system code page.
if (MSVC)
    set(TGX_WARNINGS /W4 /permissive- /utf-8)
    if (TGX_WARNINGS_AS_ERRORS)
        list(APPEND TGX_WARNINGS /WX)
    endif ()
else ()
    set(TGX_WARNINGS -Wall -Wextra -Wpedantic)
    if (TGX_WARNINGS_AS_ERRORS)
        list(APPEND TGX_WARNINGS -Werror)
    endif ()
endif ()

add_subdirectory(thirdparty)

set(TGX_SOURCES
        src/app.cpp
        src/assert.cpp
        src/audio.cpp
        src/batch.cpp
        src/canvas.cpp
        src/clock.cpp
        src/context.cpp
        src/device.cpp
        src/file.cpp
        src/image.cpp
        src/input.cpp
        src/log.cpp
        src/render_target.cpp
        src/texture.cpp
        src/window.cpp
)

set(TGX_GL_SOURCES
        src/gl/buffer.cpp
        src/gl/shader.cpp
        src/gl/vertex_array.cpp
)

add_library(tgx STATIC ${TGX_SOURCES} ${TGX_GL_SOURCES})
add_library(tgx::tgx ALIAS tgx)

target_compile_features(tgx PUBLIC cxx_std_23)

target_include_directories(tgx
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tgx PRIVATE tgx_glad glfw tgx_miniaudio tgx_stb_image tgx_stb_image_write)
target_compile_definitions(tgx PRIVATE GLFW_INCLUDE_NONE)

# Public headers hold inline code and defaults that depend on this, so the
# library and everything linking it must see the same value. By default asserts
# are off in the configurations where CMake defines NDEBUG.
# Checked by hand: if() would read a mistyped value such as ONN as a variable
# name and quietly turn asserts off.
string(TOUPPER "${TGX_ASSERTS}" tgx_asserts)
set(tgx_on_values ON TRUE YES 1)
set(tgx_off_values OFF FALSE NO 0)
if (tgx_asserts STREQUAL "")
    set(tgx_enable_asserts "$<IF:$<CONFIG:Release,RelWithDebInfo,MinSizeRel>,0,1>")
elseif (tgx_asserts IN_LIST tgx_on_values)
    set(tgx_enable_asserts 1)
elseif (tgx_asserts IN_LIST tgx_off_values)
    set(tgx_enable_asserts 0)
else ()
    message(FATAL_ERROR "TGX_ASSERTS must be ON, OFF or empty, not '${TGX_ASSERTS}'")
endif ()
target_compile_definitions(tgx PUBLIC TGX_ENABLE_ASSERTS=${tgx_enable_asserts})
target_compile_options(tgx PRIVATE ${TGX_WARNINGS})

if (TGX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (TGX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()