# Everything that draws: the Canvas, Device::draw, textures, shaders, buffers
# and render targets, checked by the pixels read back. Needs a window with a GL
# 3.3 context, so it runs where there is a display: locally, and on Linux CI
# under Xvfb with Mesa's software renderer.
add_executable(tgx_tests_gpu
        main.cpp
        buffers.cpp
        order.cpp
        read.cpp
        render_target.cpp
        shaders.cpp
        shapes.cpp
        textures.cpp
        viewport.cpp
)
target_link_libraries(tgx_tests_gpu PRIVATE tgx::tgx tgx_doctest)
# support.h, shared by all the tests.
target_include_directories(tgx_tests_gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(tgx_tests_gpu PRIVATE ${TGX_WARNINGS})

add_test(NAME tgx_tests_gpu COMMAND tgx_tests_gpu)
set_tests_properties(tgx_tests_gpu PROPERTIES LABELS gpu)
