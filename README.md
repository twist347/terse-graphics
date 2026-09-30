# terse-graphics (tgx)

A small 2D-first graphics library on top of OpenGL 3.3 core, C++23. Tested on
Linux (NVIDIA, Mesa) and macOS; Windows is a target but has not been tried yet.

Early and moving: the API changes from commit to commit.

## What there is

- **Window and loop**: `App` with a frame clock, resize tracking and a GLFW-shaped
  loop.
- **Buffers and vertex arrays**: immutable or dynamic `gl::Buffer`s from any
  contiguous range; `gl::VertexArray` with the attribute layout taken straight
  from the vertex struct.
- **Shaders and uniforms**: `gl::Shader` from source, uniforms set through typed
  handles looked up once by name.
- **Render state per draw**: blending, depth test, face culling, wireframe.
- **Math**: `Vec2`/`Vec3`/`Vec4`/`Mat4` laid out for the GPU, with the usual
  operations and `ortho`, `perspective`, `look_at`, `translate`, `rotate`,
  `scale`. Same conventions as glm, and glm values convert with `std::bit_cast`.
- **Checks**: asserts for the mistakes GL keeps quiet about (see below), and GL
  driver messages routed to the log on debug contexts.

Not yet: textures, the `Canvas` for simple 2D drawing, input, render targets,
text.

## Who does what

| Object     | Owns                                                                                   |
|------------|----------------------------------------------------------------------------------------|
| `App`      | The simple way in: one `Platform`, `Window` and `Device` plus a frame `Clock`, created together and torn down in the right order. |
| `Platform` | `glfwInit`/`glfwTerminate` and event polling. Knows nothing about GL.                  |
| `Window`   | The OS window and its GL context: version hints, making it current, swap, vsync.     |
| `Device`   | Loads GL functions, checks the version, installs the debug callback (where `KHR_debug` exists), logs what context the driver gave. Then everything that changes global GL state or draws: clear, viewport, render state, draw calls. |
| `gl::*`    | Raw resources (`Buffer`, `VertexArray`, `Shader`): create, fill, destroy. Editing may bind the resource (3.3 has no DSA), but never where a draw would read it. |

Creation order is the dependency chain, and each step fails on its own:

    auto platform = tgx::Platform::create();
    auto window   = tgx::Window::create(*platform, {...});
    auto device   = tgx::Device::create(*window);

`App` does the same in one call. Its frame loop has the shape of a plain GLFW
one; `poll_events` also fits the viewport after a resize, `swap_buffers` also
ticks the clock:

    auto app = tgx::App::create({.title = "tgx"});
    while (!app->should_close()) {
        app->poll_events();

        app->device().clear();
        app->device().draw(*shader, vao);

        app->swap_buffers();
    }

## Drawing

A vertex is a plain struct; the layout comes from its fields, format and offset
alike:

    struct Vertex {
        tgx::Vec2 position;
        tgx::Color color;
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),   // float32x2
        tgx::gl::VertexAttribute::of(1, &Vertex::color),      // unorm8x4, read as vec4 in [0, 1]
    };

    auto vbo = tgx::gl::Buffer::create(device, vertices);   // std::array, std::vector, span...
    auto vao = tgx::gl::VertexArray::create<Vertex>(device, layout);
    vao.set_vertex_buffer(*vbo);

Shader sources carry their own `#version` line; `TGX_GLSL_VERSION` is the one
matching the context:

    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        ...
    )";

    std::string log;   // compile and link messages, warnings included
    auto shader = tgx::gl::Shader::from_source(device, vertex_source, fragment_source, &log);

Uniforms are looked up by name once, then set without names. The handle's type
is checked against the GLSL type when it is looked up, and against the value by
the compiler:

    const auto u_mvp  = shader->uniform<tgx::Mat4>("u_mvp");
    const auto u_tint = shader->uniform<tgx::Color>("u_tint");   // a vec4 in GLSL

    shader->set(u_mvp, projection * view * model);
    shader->set(u_tint, tgx::colors::red.with_alpha(128));

A draw draws the whole buffer unless told otherwise, and carries its own render
state. Nothing one draw sets leaks into the next; `Device` only changes what
differs from the previous draw:

    device.draw(*shader, vao);
    device.draw(*shader, vao, {.count = 6, .first = 12});
    device.draw(*shader, sprites, {.state = {.blend = tgx::Blend::alpha}});
    device.draw(*shader, cube, {.state = {.depth = tgx::Depth::less, .cull = tgx::Cull::back}});

`Blend` has `none`, `alpha`, `premultiplied`, `additive` and `multiply`; `Depth`
has `none`, `less` and `less_equal`, plus `depth_write`; `Cull` has `none`,
`back` and `front`; `Fill` has `solid` and `wireframe`.

## Rules

- **Only `Device` binds for drawing.** Resources have no `bind()`. They may bind
  themselves to be edited, so `Device` assumes nothing about what they leave
  bound; it only skips what it set itself and knows to be current (the program,
  the render state).
- **A resource is an object; `Device` is how and with what we draw right now.**
- **No GL enums or concepts leak into the public API**, except in the `gl::` layer,
  which is the deliberate escape hatch.
- **`Result` for failures from outside** (driver, OS, files); **asserts for caller
  mistakes** (`TGX_ASSERT`, controlled by `TGX_ENABLE_ASSERTS`, not `NDEBUG`).
- **One `Window`, one `Device`** at a time (asserted).
- **`gl::*` resources must be destroyed before the `Device`**: after it the GL
  context is gone (asserted). Declare them after the `App`.
- **tgx does not log what it returns.** A failure goes out as a `Result` only;
  the log is for what a `Result` cannot carry. Messages from the GL driver and
  GLFW always go to the log, even when the same failure also comes back as a
  `Result` (e.g. GLFW explaining why `Window::create` failed).

## What the asserts catch

GL accepts most mistakes without a word and draws garbage, or nothing. With
asserts on, tgx stops at the call instead:

- a draw reading past the end of its vertex or index buffer;
- a vertex shader input with no attribute in the vertex array, or an integer
  input fed floats (and the other way round);
- a uniform that is misspelled or was optimized out, of another GLSL type than
  the handle, or set through a handle from another shader;
- a vertex layout that does not fit its stride, or uses a location twice;
- writing to an immutable buffer, or past the end of a dynamic one;
- a second `Window` or `Device`, or GL resources outliving the `Device`.

Asserts cost nothing when off: the checks that need extra bookkeeping (such as
reading the shader's inputs) are skipped altogether.

## Logging

By default only `warn` and `error` are logged, so a working program is silent.
Messages go to the console: `info` and `debug` to stdout, `warn` and `error` to
stderr, one line each, prefixed with `[tgx] <level>:`. Both the level and the
destination are configurable, before creating the `App`:

    tgx::set_log_level(tgx::LogLevel::info);   // also show the GL context at startup; off drops all
    tgx::set_log_sink([](tgx::LogLevel level, std::string_view msg, void *) noexcept {
        // forward to your own logger
    });

A sink may be called from a driver thread. Passing `nullptr` restores the default.

Driver messages are logged by their severity, except performance hints, which
are advice rather than faults and go to `info`. A debug context, and with it the
driver messages, is on by default only in builds with asserts
(`WindowParams::debug_context`).

## Examples

| Example        | Shows                                                              |
|----------------|--------------------------------------------------------------------|
| `01_window`    | The bare loop: a cleared window with the frame rate in its title.  |
| `02_triangle`  | Vertices, a vertex array and a shader.                             |
| `03_rectangle` | An index buffer.                                                   |
| `04_circle`    | An orthographic projection that keeps the circle round on resize.  |
| `05_uniforms`  | Uniforms of several types, moving a triangle on the GPU.           |
| `06_blend`     | The five blend modes side by side.                                 |
| `07_cube`      | 3D: perspective, a camera, depth test and back-face culling.       |

## Building

CMake 3.25+ and a C++23 compiler. GLFW and glad are vendored in `thirdparty/`.

    cmake -S . -B build
    cmake --build build

Examples are built by default; turn them off with `-DTGX_BUILD_EXAMPLES=OFF`.
Asserts follow the build type (off where CMake defines `NDEBUG`); force them
with `-DTGX_ASSERTS=ON` or `OFF`. The setting reaches everything that links
`tgx::tgx`, so the library and the app always agree.
In another CMake project: `add_subdirectory(terse-graphics)` and link `tgx::tgx`.
