# Changelog

## Unreleased

### Changed

- **Text measurement and drawing share a font-independent layout.** Glyph
  metrics and built-in atlas construction are separated from the GPU batch;
  the public API and the built-in font's output are unchanged.
- **`gl::VertexArray::create` returns `Result<VertexArray>`.** Both overloads
  report driver failures as `Error::out_of_memory` or `Error::platform`.
  Check the result and dereference it before attaching buffers or drawing.

### Fixed

- **`Image::save` no longer shares a fixed temporary file.** Each save creates
  its own file exclusively beside the destination, so an existing
  `.tgx-partial` sibling is left alone and concurrent saves do not interfere.
- **Window depth checks use the buffer the driver provided.** Its presence
  and bit depth are queried at device setup and logged; depth clears and
  draws no longer assume the window has a depth buffer.
- **`Window`, `Device` and `Audio` can no longer be assigned.** In
  `device = tgx::Device::create(*window)` the new device was made first, and
  then the old one released the state the two share: the next draw crashed.
  A `Window` assigned this way was destroyed, and an `Audio` went silent
  while the old output kept running on freed memory. They still move.
- **The guide no longer promises game code reloaded while the game runs.** A
  shared library that links tgx gets its own copy, with its own state:
  without an `App` of its own it sees no window, and its first draw crashes.
  Sharing one tgx needs a shared build, now on the roadmap.

### Tests

- Known text pixels, indexed draw ranges for 16- and 32-bit indices,
  texture update offsets, target switch counts, camera rotation direction
  and depth clears after draws that disable depth writes.
- Shader compile and link failures, stage labels and replacement of logs.
- Unexpected GL/GLFW warnings and errors fail the GPU run. Debug contexts
  are requested in Release too; expected shader compiler messages are scoped
  to the invalid shader calls.
- The FPS-limit test checks a series of frames, allowing a short frame
  after a late one and arbitrary scheduler delays.
- GPU test queries (help, lists, counts and `--no-run`) need no display.
- Save-directory tests use temporary data folders on every platform.
- Color rounding, easing between endpoints, UTF-8 boundaries, circle
  segment counts and the center of a four-color gradient.
- Window depth detection, including a reported missing attachment.
- CTest timeouts: 60 seconds for the CPU suite, 120 for the GPU suite.
- Input transitions, short taps, repeats, cursor settling, UTF-8 text, GLFW
  key translation and the installed input callbacks.
- Vertex array allocation failures, cleanup, preservation of the previously
  bound layout and subsequent drawing.
- Compile-time checks that `Window`, `Device` and `Audio` move but do not
  assign.

## 0.1.0 — 2026-10-21

The first release. Before 1.0 a minor version may break the API; each break
is listed here, and `find_package(tgx 0.1)` accepts only 0.1.x.

### Contents

- **Window and loop** — `App`, `Window` (fullscreen, focus, minimized, vsync,
  a frame rate limit, MSAA, resizable, cursor modes and shapes), `Device`
  (render state, draws, `FrameStats`), `Clock`, `Timer`, paths for the
  program's folder and saves
- **Input** — keys, mouse buttons, the wheel, typed text, `axis` and
  `direction`
- **Canvas** — rects (rounded too), circles, sectors, rings, ellipses,
  triangles, polygons, lines and strips, gradients, outlines, sprites,
  nine-slice, text in a built-in pixel font; `Camera2D`, viewport, clip rect,
  blend modes, letterboxed logical size; batched draws
- **Render targets** — pixel art scaled up, post effects, reading pixels back
- **Images and textures** — PNG, JPEG, BMP, TGA, GIF in, PNG out; filter,
  wrap, updates
- **Audio** — `Sound` (WAV, Ogg Vorbis, MP3, FLAC, and tones made in code),
  streamed `Music`, `Playback` to pause or stop one play, pitch, pan, volume
- **Utilities** — `Vec2/3/4`, `Mat4`, `Rect`, `Color`, collisions, `Random`
  (PCG32), easing, `remap`, `smoothstep`, `wrap`, `Result` errors, logging,
  asserts
- **`tgx::gl`** — shaders with reflected uniforms and samplers, typed
  buffers, vertex arrays, texture slots, draws in the same frame as the
  canvas
- **Build** — CMake 3.25+, C++23, every dependency vendored; GCC, Clang and
  MSVC; `add_subdirectory` or `cmake --install` + `find_package(tgx)`; glad
  and GLFW headers exported for raw GL, stb and miniaudio kept inside
- **Examples** — 16 lessons on the `tgx` level, 8 on the `gl` level,
  6 scenes, 2 games

### Known issues

- macOS ignores vsync: frames run as fast as they can unless an fps limit is
  set.
- Canvas units are logical pixels on macOS and Wayland but physical ones on
  Windows and X11 (HiDPI, on the roadmap).
