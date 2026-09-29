# terse-graphics (tgx)

A small 2D-first graphics library on top of OpenGL 4.5 core (DSA everywhere), C++23.

## Who does what

| Object     | Owns                                                                                   |
|------------|----------------------------------------------------------------------------------------|
| `App`      | The simple way in: one `Platform`, `Window` and `Device` plus a frame `Clock`, created together and torn down in the right order. |
| `Platform` | `glfwInit`/`glfwTerminate` and event polling. Knows nothing about GL.                  |
| `Window`   | The OS window and its GL context: version hints, making it current, swap, vsync. Hands out the loader for GL functions. |
| `Device`   | Loads GL functions, checks the version, installs the debug callback, logs what context the driver gave. Then everything that changes global GL state or draws: clear, viewport, blending, depth, scissor, binding shaders/textures/VAOs, the render target, draw calls. |
| `gl::*`    | Raw resources (`Texture`, `Buffer`, `Shader`, ...): create, fill, destroy. Thanks to DSA this never touches global state. |

Creation order is the dependency chain, and each step fails on its own:

    auto platform = tgx::Platform::create();
    auto window   = tgx::Window::create(*platform, {...});
    auto device   = tgx::Device::create(*window);

`App` does the same in one call and runs the frame loop:

    auto app = tgx::App::create({.title = "tgx"});
    while (app->next_frame()) {
        app->device().clear();
        app->device().draw(*shader, vao, {.count = 3});
    }

See `example/` for complete programs.

## Rules

- **Only `Device` binds.** Resources have no `bind()`. The sprite batch relies on
  nothing changing GL state between its draws.
- **A resource is an object; `Device` is how and with what we draw right now.**
- **No GL enums or concepts leak into the public API**, except in the `gl::` layer,
  which is the deliberate escape hatch.
- **`Result` for failures from outside** (driver, OS, files); **asserts for caller
  mistakes** (`TGX_ASSERT`, controlled by `TGX_ENABLE_ASSERTS`, not `NDEBUG`).
- **The log is only for what a `Result` cannot carry**: driver debug messages,
  GLFW errors, context info. A failure that is returned is not logged too.

## Logging

By default messages go to the console: `info` and `debug` to stdout, `warn` and
`error` to stderr, one line each, prefixed with `[tgx] <level>:`. Both are
configurable, before creating the `App`:

    tgx::set_log_level(tgx::LogLevel::warn);   // drop info and debug; off drops all
    tgx::set_log_sink([](tgx::LogLevel level, std::string_view msg, void *) noexcept {
        // forward to your own logger
    });

A sink may be called from a driver thread. Passing `nullptr` restores the default.

## Building

CMake 3.25+ and a C++23 compiler. GLFW and glad are vendored in `thirdparty/`.

    cmake -S . -B build
    cmake --build build

Examples are built by default; turn them off with `-DTGX_BUILD_EXAMPLES=OFF`.
In another CMake project: `add_subdirectory(terse-graphics)` and link `tgx::tgx`.
