# A project of its own that takes tgx in the two ways a game would, into an
# executable and into a shared library of its own. An installed package:
#
#     cmake --install build --prefix <dir>
#     cmake -S tests/install -B build-install -DCMAKE_PREFIX_PATH=<dir>
#     cmake --build build-install && build-install/tgx_install_check
#
# Or the source, built inside it with add_subdirectory:
#
#     cmake -S tests/install -B build-subdirectory -DTGX_SOURCE_DIR=<tgx checkout>
#
# Not part of tgx's build; CI runs both.
cmake_minimum_required(VERSION 3.25)

project(tgx_install_check LANGUAGES CXX)

set(TGX_SOURCE_DIR "" CACHE PATH "A tgx checkout to build with add_subdirectory instead of finding the package")
if (TGX_SOURCE_DIR)
    add_subdirectory(${TGX_SOURCE_DIR} tgx)
else ()
    find_package(tgx 0.1 REQUIRED)
endif ()

add_executable(tgx_install_check main.cpp)
target_link_libraries(tgx_install_check PRIVATE tgx::tgx)

# tgx inside a shared library of the app's own, such as a plugin: links only
# if tgx is position-independent.
add_library(tgx_install_plugin MODULE plugin.cpp)
target_link_libraries(tgx_install_plugin PRIVATE tgx::tgx)
