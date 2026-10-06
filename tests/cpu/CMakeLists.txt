# Math, colors, collisions, the camera, random numbers, images, sounds,
# errors and log levels as text, measuring text, the arithmetic under drawing
# and input (src/core/geometry.h), and what an App hands out (at compile time):
# nothing here opens a window or an audio output, so it runs anywhere, CI
# without a display or a sound card included.
add_executable(tgx_tests_cpu
        main.cpp
        app.cpp
        audio.cpp
        camera.cpp
        collision.cpp
        color.cpp
        error.cpp
        geometry.cpp
        image.cpp
        math.cpp
        random.cpp
        text.cpp
)
target_link_libraries(tgx_tests_cpu PRIVATE tgx::tgx tgx_doctest)
# support.h, shared by all the tests, and tgx's internal geometry.h, which
# needs nothing but public headers.
target_include_directories(tgx_tests_cpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${PROJECT_SOURCE_DIR}/src)
target_compile_options(tgx_tests_cpu PRIVATE ${TGX_WARNINGS})

add_test(NAME tgx_tests_cpu COMMAND tgx_tests_cpu)
set_tests_properties(tgx_tests_cpu PROPERTIES LABELS cpu)
