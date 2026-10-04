# terse-graphics (tgx)

A small 2D-first graphics library on top of OpenGL 3.3 core, C++23. Tested on
Linux (NVIDIA, Mesa) and macOS; Windows is a target but has not been tried yet.

Early and moving: the API changes from commit to commit.

## Two levels

- **`tgx`** (`#include "tgx/tgx.h"`): a window, a loop and 2D drawing on the
  `Canvas`, with images and textures, and the `Device` that shows the frames.
  Normal use needs nothing from OpenGL; enough for a game in the spirit of
  raylib.
- **`tgx::gl`** (`#include "tgx/gl.h"`): drawing with your own shaders and
  buffers through `Device::draw`. Everything in it is OpenGL. It builds on
  `tgx` and mixes with the `Canvas` in the same frame.

The levels split the headers, not the library: it is one library. The `Device`
holds the `Canvas`'s batch, so it always makes it (a shader, an 858x16 texture
with the built-in font, about 420 KB of buffers), even for a program that draws only with its
own shaders.

## What there is

In `tgx`:

- **Window and loop**: `App` with a GLFW-shaped loop; resizes noted as events
  are polled (`window().resized()`), frames timed as they are presented
  (`clock()`: delta, elapsed, fps), and `canvas.fps(pos)` to show them.
- **Input**: keyboard and mouse as of the last poll: keys and buttons held,
  pressed or released this frame, the mouse in window coordinates (and through
  `Canvas::to_world` in the world), the wheel, typed text in UTF-8.
- **Canvas**: 2D drawing in screen coordinates: rectangles, triangles, lines,
  circles, outlines, sprites (parts of textures, mirrored, turned, tinted) and
  text in a built-in pixel font, under a 2D camera, with a choice of blending. Collected and drawn in
  batches, always in the order of the calls.
- **Collisions**: `contains` (a point in a rect, circle or triangle),
  `overlaps` (rects and circles, in any pair), `intersection` (the part two
  rects share, the point two segments cross at), in `tgx/collision.h`.
- **Images and textures**: `Image` (RGBA8 pixels in memory, loaded from PNG,
  JPEG, BMP, TGA or GIF, or made in code) and `Texture` made from it, with
  nearest or linear filtering, wrapping, optional mipmaps and partial updates.
- **Math**: `Vec2`/`Vec3`/`Vec4`/`Mat4`/`Rect` laid out for the GPU, with the
  usual operations and `ortho`, `perspective`, `look_at`, `translate`, `rotate`,
  `scale`. Same conventions as glm, and glm values convert with `std::bit_cast`.
- **Checks**: asserts for the mistakes GL keeps quiet about (see below), and GL
  driver messages routed to the log on debug contexts.

In `tgx::gl`:

- **Draws and clears** that state everything they need: render state (blending,
  depth test, face culling, wireframe), textures by slot, clear values.
- **Buffers and vertex arrays**: immutable or dynamic `gl::Buffer`s from any
  contiguous range; `gl::VertexArray` with the attribute layout taken straight
  from the vertex struct.
- **Shaders and uniforms**: `gl::Shader` from source, uniforms set through typed
  handles looked up once by name; a shader of your own for the `Canvas`.

Not yet: fonts of your own (TTF), render targets.

## Who does what

| Object        | Owns                                                                                   |
|---------------|----------------------------------------------------------------------------------------|
| `App`         | The simple way in: one `Window`, `Device` and `Canvas`, created together and torn down in the right order. |
| `Window`      | `glfwInit`/`glfwTerminate`, the OS window and its GL context: version hints, making it current, polling events, size (kept up to date as GLFW reports it) and whether it changed, title, vsync, closing; keyboard and mouse state, shown by `Input`. Knows nothing else about GL. |
| `Device`      | Loads GL functions, checks the version, installs the debug callback (where `KHR_debug` exists), logs what context the driver gave. Then everything that changes global GL state or draws: clear, render state, draw calls (`draw` is the one part of the `gl` level), presenting frames and timing them (`Clock`), and the batch of 2D vertices the `Canvas` fills, drawn before anything else of its own. |
| `Canvas`      | Simple 2D drawing: turns shapes and sprites into vertices for the `Device` to draw in as few draws as it can. Holds no GPU resources, only how to draw (size, camera, blend, shader): a plain value to copy. |
| `Texture`     | An image on the GPU, for the `Canvas` and `Device::draw` alike. Nothing GL-specific to configure; `id()` is the way out to raw GL. Editing binds it through the `Device`'s cache, so the next draw still finds what it asks for. |
| `gl::*`       | Raw resources (`Buffer`, `VertexArray`, `Shader`): create, fill, destroy. Editing may bind the resource (3.3 has no DSA), but never where a draw would read it. |

GPU resources (`Texture`, `gl::*`) are created without naming the `Device`, but
only while it exists, and destroyed before it. Declare them after the `App`.

`App` creates everything in one call. Its frame loop has the shape of a plain
GLFW one. Each part keeps its own state up to date where it changes: polling
events refreshes the window's sizes, `app->window().resized()` and
`app->input()`; presenting the frame times it for `app->clock()`. Nothing needs
fitting after a resize: draws cover the whole framebuffer unless told
otherwise, and the canvas follows the window.

    auto app = tgx::App::create({.title = "tgx"});
    while (!app->should_close()) {
        app->poll_events();

        app->canvas().clear(tgx::colors::black);
        ...

        app->swap_buffers();
    }

The parts can also be created one by one; creation order is the dependency
chain, and each step fails on its own:

    auto window = tgx::Window::create({...});
    auto device = tgx::Device::create(*window);
    auto canvas = tgx::Canvas::create();   // follows the window

Frames are shown with `device->present()`, which also draws the last of the
`Canvas` shapes; `App::swap_buffers` calls it.

## Canvas

The quick way to draw in 2D: no shaders, buffers or vertex arrays. Coordinates
are the window's screen coordinates, (0, 0) at the top-left, y down; on a
scaling display (Retina) things keep their size.

    auto &canvas = app->canvas();
    while (!app->should_close()) {
        app->poll_events();

        canvas.clear(tgx::colors::dark_gray);
        canvas.rect({40, 40, 90, 90}, tgx::colors::red);
        canvas.triangle({190, 200}, {320, 360}, {60, 360}, tgx::colors::green);
        canvas.line({400, 200}, {640, 200}, tgx::colors::white, 4.f);
        canvas.circle({800, 300}, 60.f, tgx::colors::yellow);
        canvas.circle_lines({800, 300}, 80.f, tgx::colors::white, 2.f);
        canvas.rect_lines({40, 400, 200, 100}, tgx::colors::cyan, 3.f);

        app->swap_buffers();
    }

A canvas can instead have a size of its own, a fixed logical resolution
stretched over the window:

    tgx::Canvas pixels = app->canvas();
    pixels.set_size({320, 180});   // set_size({}) follows the window again

It can also cover only a part of the window, for a minimap or a split screen;
its coordinates then span that part:

    tgx::Canvas minimap = app->canvas();
    minimap.set_viewport({16, 16, 200, 150});   // screen coordinates of the window

Outlines lie inside the shape they outline, so a frame and a fill of the same
rectangle cover the same area.

Shapes are collected and drawn together, but the picture always follows the
order of the calls: the `Device` collects them and draws them before any draw
or clear of its own and before presenting the frame.
Nothing the shapes use is read later than the calls that made them: a texture
updated or destroyed, or a uniform of the canvas shader set, has the
shapes waiting on it drawn first. `canvas.flush()` is only needed before raw
GL calls.

Images load from files or are made in code; rows run top to bottom. A sprite
needs only a position: by default it is the whole texture at its own size.

    auto image = tgx::Image::load("player.png");   // Error::io or Error::decode on failure
    auto player = tgx::Texture::create(*image, {.filter = tgx::TextureFilter::nearest});

    canvas.sprite(*player, {pos});
    canvas.sprite(*atlas, {
        .position = pos,
        .size = {64, 64},              // on the canvas; default: the size of src
        .src = {16, 0, 16, 16},        // texels; default: the whole texture; negative size mirrors
        .origin = {32, 32},            // the point at position, and what rotation turns around
        .rotation = angle,             // radians, clockwise
        .tint = tgx::colors::red,
    });

A camera moves, turns and zooms the world. A canvas is a plain value holding
how to draw (size, camera, blend, shader), so rather than switching one back
and forth, keep a copy for each way of drawing. Copies share the frame and
the order of the calls:

    tgx::Canvas world = app->canvas();
    world.set_camera({.target = player_pos, .offset = screen_center, .zoom = 2.f});

    world.sprite(...);                  // the world
    app->canvas().rect(...);            // the HUD, on top

    tgx::Canvas glow = app->canvas();
    glow.set_blend(tgx::Blend::additive);   // Blend::alpha by default

`canvas.to_world(point)` gives the world point under a point of the window,
such as the mouse, and `to_screen(world)` the way back; they go through the
canvas's viewport and size as well as its camera, so they hold for a minimap
or a pixel-art canvas too.

Text comes in a built-in monospaced pixel font, [unscii-16](http://viznut.fi/unscii/)
by Viznut (public domain): 8x16 pixels a character, printable ASCII; other
characters show as `?`. Its own size is 16, the height of a line; whole
multiples of it keep the pixels square.

    canvas.text({20, 20}, "Score: 120", tgx::colors::white);
    canvas.text({20, 60}, "GAME OVER", tgx::colors::red, 48);
    const tgx::Vec2 extent = tgx::Canvas::measure_text("GAME OVER", 48);   // to center it

The font shares its texture with the shapes, so text and shapes drawn in a row
are one draw. It is baked into the library by `tools/bake_font.py` from
`thirdparty/unscii/`.

Shapes in a row with the same texture, camera, blend and shader go out as one
draw; a change of any of them starts the next. So many sprites from one texture
(an atlas) cost one draw. A texture updated or destroyed while its sprites wait
has them drawn first; moving one changes nothing.

## Custom drawing (`tgx::gl`)

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

    auto vbo = tgx::gl::Buffer::create(vertices);   // std::array, std::vector, span...
    auto vao = tgx::gl::VertexArray::create<Vertex>(layout);
    vao.set_vertex_buffer(*vbo);

Shader sources carry their own `#version` line; `TGX_GLSL_VERSION` is the one
matching the context:

    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        ...
    )";

    std::string log;   // compile and link messages, warnings included
    auto shader = tgx::gl::Shader::from_source(vertex_source, fragment_source, &log);

Uniforms are looked up by name once, then set without names. The handle's type
is checked against the GLSL type when it is looked up, and against the value by
the compiler:

    const auto u_mvp  = shader->uniform<tgx::Mat4>("u_mvp");
    const auto u_tint = shader->uniform<tgx::Color>("u_tint");   // a vec4 in GLSL

    shader->set(u_mvp, projection * view * model);
    shader->set(u_tint, tgx::colors::red.with_alpha(128));

A draw draws the whole buffer unless told otherwise, and carries its own render
state; a clear carries its own values. Nothing one call sets leaks into the
next; the `Device` only changes what differs from the previous call. That
includes the viewport: the whole framebuffer unless a draw says otherwise.

    auto &device = app->device();
    device.clear({.color = tgx::colors::black, .depth = 1.f});
    device.draw(*shader, vao);
    device.draw(*shader, vao, {.count = 6, .first = 12});
    device.draw(*shader, sprites, {.state = {.blend = tgx::Blend::alpha}});
    device.draw(*shader, cube, {.state = {.depth = tgx::gl::Depth::less, .cull = tgx::gl::Cull::back}});
    device.draw(*shader, minimap, {.viewport = tgx::gl::Viewport{0, 0, 256, 256}});   // pixels, from the top-left

Textures are the same `tgx::Texture` the `Canvas` takes; texture coordinates
(0, 0) are the image's top-left pixel. A `sampler2D` uniform is set once to a
slot, and each draw puts textures into slots:

    shader->set(shader->uniform<tgx::gl::TextureSlot>("u_texture"), {0});
    device.draw(*shader, quad, {.textures = {&*texture}});

The `Canvas` can draw through a shader of your own, which takes what its own
takes (see `Canvas::set_shader`); its other uniforms are yours to set:

    tgx::Canvas gray = app->canvas();
    gray.set_shader(&*grayscale);
    gray.rect(...);   // through grayscale

`Blend` has `none`, `alpha`, `premultiplied`, `additive` and `multiply` (the last
takes premultiplied colors, as `premultiplied` does: `Color::premultiplied()`); `Depth`
has `none`, `less` and `less_equal`, plus `depth_write`; `Cull` has `none`,
`back` and `front`; `Fill` has `solid` and `wireframe`.

## Rules

- **Only `Device` binds for drawing.** Resources have no `bind()`. They may bind
  themselves to be edited, so `Device` assumes nothing about what they leave
  bound; it only skips what it set itself and knows to be current (the program,
  the textures in their slots, the render state).
- **A resource is an object; `Device` is how and with what we draw right now.**
- **Everything counts from the top-left, y down**: image rows, texture
  coordinates, the `Canvas`, `gl::Viewport`. Where GL counts from the
  bottom-left (the viewport), tgx turns it around inside.
- **`tgx` needs no `gl` for normal use.** GL types, enums and concepts live in
  `tgx::gl`. The few ways out to GL from `tgx` (`Texture::id()`,
  `Canvas::set_shader`) are marked as such.
- **`Result` for failures from outside** (driver, OS, files); **asserts for caller
  mistakes** (`TGX_ASSERT`, controlled by `TGX_ENABLE_ASSERTS`, not `NDEBUG`).
- **Out of memory: GPU memory is a failure, host memory is fatal.** A buffer or
  texture the driver has no room for comes back as `Error::out_of_mem`. Host
  memory running out is not reported: the functions that allocate as much as
  their input asks for (`Image::create`, `from_pixels`, `load`, `decode`,
  `gl::Shader::from_source`) may throw `std::bad_alloc`, and everywhere else,
  `noexcept` included, it ends the program.
- **One `Window`, one `Device`.** They are one per process by
  nature (GLFW, the GL context of the one window), so tgx keeps their state in
  one place each and the objects only own it.
- **Lifetimes nest**: `Window` > `Device` > GPU resources, each
  created after and destroyed before the one it lives in. `App` declares them in
  that order. Like the standard library, tgx does not check this, nor calls on
  moved-from objects: asserts are for arguments, not for bookkeeping.
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
- a sampler reading a texture slot the draw put no texture in, or a slot out of
  range;
- a vertex layout that does not fit its stride, or uses a location twice;
- writing to an immutable buffer or texture, or past the end of a dynamic one;
- a pixel outside an `Image`;
- a canvas shader without `u_projection`, or with a sampler other than
  `u_texture`.

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

A sink is called on the thread that made the tgx call, GL driver messages
included: they are delivered synchronously. Passing `nullptr` restores the default.

Driver messages are logged by their severity, except performance hints, which
are advice rather than faults and go to `info`. A debug context, and with it the
driver messages, is on by default only in builds with asserts
(`WindowParams::debug_context`).

## Examples

Lessons, one idea each, in two folders by level: `examples/tgx/` includes only
`tgx/tgx.h`, `examples/gl/` adds `tgx/gl.h`. Each builds as `<level>_<name>`,
e.g. `tgx_01_window`.

| `examples/tgx/` | Shows                                                              |
|-----------------|--------------------------------------------------------------------|
| `01_window`     | The bare loop: a window cleared to one color.                      |
| `02_shapes`     | The `Canvas` shapes, one call each: filled, outlined, see-through. |
| `03_texture`    | An `Image` made pixel by pixel, as a `Texture`, drawn as a sprite. |
| `04_sprite`     | The `Sprite` fields: part of an atlas, mirrored, tinted, turned.   |
| `05_moving`     | Movement by `clock().delta()`, the same at any frame rate.         |
| `06_camera`     | A camera following a player, and a HUD that stays put.            |
| `07_viewport`   | A minimap: the world again, in a corner, at its own scale.         |
| `08_blend`      | Alpha and additive blending side by side.                          |
| `09_input`      | Keyboard and mouse: held keys, presses, the wheel, the mouse in the world. |
| `10_text`       | Text in the built-in font: sizes, lines, centering, a field to type in. |
| `11_collision`  | Collision checks: a point in a shape, shapes overlapping, the part rects share. |

| `examples/gl/`     | Shows                                                           |
|--------------------|-----------------------------------------------------------------|
| `01_triangle`      | Vertices in a buffer, a vertex array, a shader and a draw.      |
| `02_indexed`       | An index buffer.                                                |
| `03_uniforms`      | Uniforms: a color set once, a transform set every frame.        |
| `04_texture`       | A texture on a square: uvs, a sampler and a texture slot.       |
| `05_blend`         | Render state per draw: the same squares without and with blending. |
| `06_cube`          | 3D: perspective, a camera, depth test and back-face culling.    |
| `07_canvas_shader` | The `Canvas` drawing through a shader of your own.              |

## Building

CMake 3.25+ and a C++23 compiler. GLFW, glad and stb_image are vendored in
`thirdparty/`, as is the TTF the built-in font is baked from (not built; only
`tools/bake_font.py`, with Pillow, reads it).

    cmake -S . -B build
    cmake --build build

Examples are built by default; turn them off with `-DTGX_BUILD_EXAMPLES=OFF`.
Asserts follow the build type (off where CMake defines `NDEBUG`); force them
with `-DTGX_ASSERTS=ON` or `OFF`. The setting reaches everything that links
`tgx::tgx`, so the library and the app always agree.
In another CMake project: `add_subdirectory(terse-graphics)` and link `tgx::tgx`.
