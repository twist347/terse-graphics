# Math, colors, collisions, the camera, random numbers, images, sounds,
# errors and log levels as text, measuring text, the arithmetic under drawing
# and input (src/core/geometry.hpp), and what an App hands out (at compile time):
# nothing here opens a window or an audio output, so it runs anywhere, CI
# without a display or a sound card included.
tgx_add_test(tgx_tests_cpu LABEL cpu INTERNAL SOURCES
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
