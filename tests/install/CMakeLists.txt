# A project of its own that finds an installed tgx, the way a game would:
#
#     cmake --install build --prefix <dir>
#     cmake -S tests/install -B build-install -DCMAKE_PREFIX_PATH=<dir>
#     cmake --build build-install && build-install/tgx_install_check
#
# Not part of tgx's build; CI runs it after installing.
cmake_minimum_required(VERSION 3.25)

project(tgx_install_check LANGUAGES CXX)

find_package(tgx 0.1 REQUIRED)

add_executable(tgx_install_check main.cpp)
target_link_libraries(tgx_install_check PRIVATE tgx::tgx)
