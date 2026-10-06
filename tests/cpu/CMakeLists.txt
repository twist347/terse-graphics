# Math, colors, collisions, the camera, images, sounds, measuring text:
# nothing here opens a window or an audio output, so it runs anywhere, CI
# without a display or a sound card included.
add_executable(tgx_tests_cpu
        main.cpp
        audio.cpp
        camera.cpp
        collision.cpp
        color.cpp
        error.cpp
        image.cpp
        math.cpp
        random.cpp
        text.cpp
)
target_link_libraries(tgx_tests_cpu PRIVATE tgx::tgx tgx_doctest)
# support.h, shared by all the tests.
target_include_directories(tgx_tests_cpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(tgx_tests_cpu PRIVATE ${TGX_WARNINGS})

add_test(NAME tgx_tests_cpu COMMAND tgx_tests_cpu)
set_tests_properties(tgx_tests_cpu PROPERTIES LABELS cpu)
