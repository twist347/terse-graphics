# terse-graphics (tgx)

A small 2D-first graphics library on top of OpenGL 4.5 core (DSA everywhere), C++23.

## Who does what

| Object     | Owns                                                                                   |
|------------|----------------------------------------------------------------------------------------|
| `Platform` | `glfwInit`/`glfwTerminate` and event polling. Knows nothing about GL.                  |
| `Window`   | The OS window and its GL context: version hints, making it current, swap, vsync. Hands out the loader for GL functions. |
| `Device`   | Loads GL functions, checks the version, installs the debug callback. Then everything that changes global GL state or draws: clear, viewport, blending, depth, scissor, binding shaders/textures/VAOs, the render target, draw calls. |
| `gl::*`    | Raw resources (`Texture`, `Buffer`, `Shader`, ...): create, fill, destroy. Thanks to DSA this never touches global state. |

Creation order is the dependency chain, and each step fails on its own:

    auto platform = tgx::Platform::create();
    auto window   = tgx::Window::create(*platform, {...});
    auto device   = tgx::Device::create(*window);

## Rules

- **Only `Device` binds.** Resources have no `bind()`. The sprite batch relies on
  nothing changing GL state between its draws.
- **A resource is an object; `Device` is how and with what we draw right now.**
- **No GL enums or concepts leak into the public API**, except in the `gl::` layer,
  which is the deliberate escape hatch.
- **`Result` for failures from outside** (driver, OS, files); **asserts for caller
  mistakes** (`TGX_ASSERT`, controlled by `TGX_ENABLE_ASSERTS`, not `NDEBUG`).
