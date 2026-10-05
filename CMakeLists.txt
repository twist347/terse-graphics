cmake_minimum_required(VERSION 3.25)

project(terse-graphics LANGUAGES C CXX)

option(TGX_BUILD_EXAMPLES "Build tgx examples" ON)
option(TGX_BUILD_TESTS "Build tgx tests" ON)
set(TGX_ASSERTS "" CACHE STRING "tgx asserts: ON, OFF, or empty to follow the build type")
set_property(CACHE TGX_ASSERTS PROPERTY STRINGS "" ON OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TGX_WARNINGS -Wall -Wextra -Wpedantic)

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
set_target_properties(tgx PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(tgx
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tgx PRIVATE glad glfw miniaudio stb_image)
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