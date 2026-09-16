cmake_minimum_required(VERSION 3.25)

project(terse-graphics LANGUAGES C CXX)

option(TGX_BUILD_EXAMPLES "Build tgx examples" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TGX_WARNINGS -Wall -Wextra -Wpedantic)

add_subdirectory(thirdparty)

add_library(tgx STATIC
        src/assert.cpp
        src/clock.cpp
        src/device.cpp
        src/platform.cpp
        src/window.cpp
)
add_library(tgx::tgx ALIAS tgx)

target_compile_features(tgx PUBLIC cxx_std_23)
set_target_properties(tgx PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(tgx
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tgx PRIVATE glad glfw)
target_compile_definitions(tgx PRIVATE GLFW_INCLUDE_NONE)
target_compile_options(tgx PRIVATE ${TGX_WARNINGS})

if (TGX_BUILD_EXAMPLES)
    add_subdirectory(example)
endif ()