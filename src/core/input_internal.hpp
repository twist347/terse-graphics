#pragma once

struct GLFWwindow;

// How the Window drives its input: the state and the callbacks that fill it
// live in input.cpp.
namespace tgx::detail {
    // Starts input afresh for the window just created: callbacks set, the
    // mouse where it is.
    auto attach_input(GLFWwindow *handle) noexcept -> void;

    // Forgets everything, for the window about to go: the callbacks go with it.
    auto detach_input() noexcept -> void;

    // Forgets what happened since the poll before (presses, releases, mouse
    // movement, wheel, text); called right before polling events.
    auto begin_input_frame() noexcept -> void;
}
