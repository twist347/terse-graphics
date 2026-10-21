# Changelog

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
  triangles, quads, polygons, lines and strips, gradients, outlines, sprites, nine-slice, text in a built-in pixel font;
  `Camera`, viewport, clip rect, blend modes, letterboxed logical size;
  batched draws
- **Render targets** — pixel art scaled up, post effects, reading pixels back
- **Images and textures** — PNG, JPEG, BMP, TGA, GIF in, PNG out; filter,
  wrap, updates
- **Audio** — `Sound` (WAV, OGG, MP3, FLAC, and tones made in code),
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
