cmake_minimum_required(VERSION 3.25)

project(terse-graphics VERSION 0.1.0 LANGUAGES C CXX)

# On when tgx is the project being built, off when another project adds it
# with add_subdirectory or FetchContent.
option(TGX_BUILD_EXAMPLES "Build tgx examples" ${PROJECT_IS_TOP_LEVEL})
option(TGX_BUILD_TESTS "Build tgx tests" ${PROJECT_IS_TOP_LEVEL})
option(TGX_INSTALL "Generate the install target and the tgx CMake package" ${PROJECT_IS_TOP_LEVEL})
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
        src/core/app.cpp
        src/core/assert.cpp
        src/core/audio.cpp
        src/core/batch.cpp
        src/core/canvas.cpp
        src/core/clock.cpp
        src/core/context.cpp
        src/core/device.cpp
        src/core/file.cpp
        src/core/image.cpp
        src/core/input.cpp
        src/core/log.cpp
        src/core/render_target.cpp
        src/core/texture.cpp
        src/core/window.cpp
)

set(TGX_GL_SOURCES
        src/gl/buffer.cpp
        src/gl/shader.cpp
        src/gl/vertex_array.cpp
)

add_library(tgx STATIC ${TGX_SOURCES} ${TGX_GL_SOURCES})
add_library(tgx::tgx ALIAS tgx)

target_compile_features(tgx PUBLIC cxx_std_23)

include(GNUInstallDirs)

target_include_directories(tgx
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# The copies in thirdparty/ are object libraries, built into tgx: linked for
# the build only, so an installed tgx asks for none of them. What they need
# from the system is linked by tgx instead, and travels with it.
target_link_libraries(tgx PRIVATE
        $<BUILD_INTERFACE:tgx_glad>
        $<BUILD_INTERFACE:tgx_miniaudio>
        $<BUILD_INTERFACE:tgx_stb_image>
        $<BUILD_INTERFACE:tgx_stb_image_write>
)
find_package(Threads REQUIRED)
target_link_libraries(tgx PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if (UNIX AND NOT APPLE)
    target_link_libraries(tgx PRIVATE m)
endif ()
if (TGX_OWN_GLFW)
    target_link_libraries(tgx PRIVATE $<BUILD_INTERFACE:glfw>)
    # What GLFW links privately, as an object library it passes on to no one.
    if (APPLE)
        target_link_libraries(tgx PRIVATE
                "-framework Cocoa" "-framework IOKit" "-framework QuartzCore" "-framework CoreFoundation"
        )
    elseif (UNIX)
        find_library(TGX_RT_LIBRARY rt)
        mark_as_advanced(TGX_RT_LIBRARY)
        if (TGX_RT_LIBRARY)
            target_link_libraries(tgx PRIVATE rt)
        endif ()
    endif ()
else ()
    # The project's own: an installed tgx finds it again as glfw3.
    target_link_libraries(tgx PRIVATE glfw)
endif ()
target_compile_definitions(tgx PRIVATE GLFW_INCLUDE_NONE)

# Public headers hold inline code and defaults that depend on this, so the
# library and everything linking it must see the same value. By default asserts
# are off in the configurations where CMake defines NDEBUG.
# Checked by hand: if() would read a mistyped value such as ONN as a variable
# name and quietly turn asserts off.
string(TOUPPER "${TGX_ASSERTS}" tgx_asserts)
set(tgx_on_values ON TRUE YES 1)
set(tgx_off_values OFF FALSE NO 0)
# An installed tgx keeps the value it was built with: a Debug game linking a
# Release tgx from one prefix gets the library's. Multi-config generators
# (Visual Studio) install each configuration separately, and their runtimes
# must match the game's anyway, so there it follows the game's configuration.
get_property(tgx_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
string(TOUPPER "${CMAKE_BUILD_TYPE}" tgx_build_type)
set(tgx_release_configs RELEASE RELWITHDEBINFO MINSIZEREL)
if (tgx_asserts STREQUAL "")
    set(tgx_enable_asserts "$<IF:$<CONFIG:Release,RelWithDebInfo,MinSizeRel>,0,1>")
    if (tgx_multi_config)
        set(tgx_installed_asserts ${tgx_enable_asserts})
    elseif (tgx_build_type IN_LIST tgx_release_configs)
        set(tgx_installed_asserts 0)
    else ()
        set(tgx_installed_asserts 1)
    endif ()
elseif (tgx_asserts IN_LIST tgx_on_values)
    set(tgx_enable_asserts 1)
    set(tgx_installed_asserts 1)
elseif (tgx_asserts IN_LIST tgx_off_values)
    set(tgx_enable_asserts 0)
    set(tgx_installed_asserts 0)
else ()
    message(FATAL_ERROR "TGX_ASSERTS must be ON, OFF or empty, not '${TGX_ASSERTS}'")
endif ()
target_compile_definitions(tgx PUBLIC
        $<BUILD_INTERFACE:TGX_ENABLE_ASSERTS=${tgx_enable_asserts}>
        $<INSTALL_INTERFACE:TGX_ENABLE_ASSERTS=${tgx_installed_asserts}>
)
target_compile_options(tgx PRIVATE ${TGX_WARNINGS})

if (TGX_INSTALL)
    include(CMakePackageConfigHelpers)

    get_target_property(tgx_glfw_imported glfw IMPORTED)
    if (NOT TGX_OWN_GLFW AND NOT tgx_glfw_imported)
        message(FATAL_ERROR "tgx can only be installed with its own GLFW or one found with find_package(glfw3)")
    endif ()

    set(tgx_package_dir ${CMAKE_INSTALL_LIBDIR}/cmake/tgx)
    install(TARGETS tgx EXPORT tgxTargets)
    install(DIRECTORY include/tgx TYPE INCLUDE)
    install(EXPORT tgxTargets NAMESPACE tgx:: DESTINATION ${tgx_package_dir})
    configure_package_config_file(cmake/tgxConfig.cmake.in tgxConfig.cmake INSTALL_DESTINATION ${tgx_package_dir})
    # Before 1.0 the API changes between minor versions.
    write_basic_package_version_file(tgxConfigVersion.cmake COMPATIBILITY SameMinorVersion)
    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/tgxConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/tgxConfigVersion.cmake
            DESTINATION ${tgx_package_dir}
    )
endif ()

if (TGX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (TGX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()