# Everything that draws: the Canvas, Device::draw, textures, shaders, buffers
# and render targets, checked by the pixels read back. Needs a window with a GL
# 3.3 context, so it runs where there is a display: locally, and on Linux CI
# under Xvfb with Mesa's software renderer.
tgx_add_test(tgx_tests_gpu LABEL gpu SOURCES
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
