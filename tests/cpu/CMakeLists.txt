# Math, colors, collisions, the camera, random numbers, timers, easing,
# images, sounds, errors and log levels as text, measuring text and cutting
# it into glyphs (src/canvas/text.hpp), the arithmetic under drawing
# (src/canvas/geometry.hpp), input transitions and key translation
# (src/platform/input_internal.hpp and glfw_keys.hpp), where a canvas lands
# (src/canvas/viewport.hpp), and what an App hands out (at compile time):
# nothing here opens a window or an audio output, so it runs anywhere, CI
# without a display or a sound card included.
tgx_add_test(tgx_tests_cpu LABEL cpu INTERNAL SOURCES
        main.cpp
        app.cpp
        audio.cpp
        camera.cpp
        collision.cpp
        color.cpp
        ease.cpp
        error.cpp
        geometry.cpp
        glfw_keys.cpp
        image.cpp
        input.cpp
        math.cpp
        paths.cpp
        random.cpp
        text.cpp
        timer.cpp
        viewport.cpp
)
