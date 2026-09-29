cmake_minimum_required(VERSION 3.25)

project(terse-graphics LANGUAGES C CXX)

option(TGX_BUILD_EXAMPLES "Build tgx examples" ON)
set(TGX_ASSERTS "" CACHE STRING "tgx asserts: ON, OFF, or empty to follow the build type")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TGX_WARNINGS -Wall -Wextra -Wpedantic)

add_subdirectory(thirdparty)

set(TGX_SOURCES
        src/app.cpp
        src/assert.cpp
        src/clock.cpp
        src/device.cpp
        src/log.cpp
        src/platform.cpp
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
)

target_link_libraries(tgx PRIVATE glad glfw)
target_compile_definitions(tgx PRIVATE GLFW_INCLUDE_NONE)

# Public headers hold inline code and defaults that depend on this, so the
# library and everything linking it must see the same value. By default asserts
# are off in the configurations where CMake defines NDEBUG.
if (TGX_ASSERTS STREQUAL "")
    set(tgx_enable_asserts "$<IF:$<CONFIG:Release,RelWithDebInfo,MinSizeRel>,0,1>")
elseif (TGX_ASSERTS)
    set(tgx_enable_asserts 1)
else ()
    set(tgx_enable_asserts 0)
endif ()
target_compile_definitions(tgx PUBLIC TGX_ENABLE_ASSERTS=${tgx_enable_asserts})
target_compile_options(tgx PRIVATE ${TGX_WARNINGS})

if (TGX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()