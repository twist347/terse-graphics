# tgx roadmap

Where tgx is going, one line per item. Designs are worked out when an item is
taken up; what is done leaves this file (the [guide](guide.md) and the history
show it). Back to the [README](../README.md).

## Before 1.0

Infrastructure:

- The bunnymark against raylib, re-measured: one line in the README, the
  numbers and their scope in the guide.
- A final pass over the whole API.
- The first release: tag 0.1.0 and start `CHANGELOG.md` with it (added,
  changed, fixed per version, as in terse-dsa), bumping `tgx/core/version.h`
  from then on.

Features:

- Cursor modes (hidden, captured for mouse look, shape) and a frame rate
  limit for when vsync is off.
- Shapes: a rotated and a rounded rectangle, `triangle_lines`, a closed
  `line_strip`, regular polygons, circle sectors and rings.
- Clipping to a rectangle without changing coordinates (raylib's scissor
  mode).
- Nine-slice sprites, for UI frames that stretch.
- Control of one playing sound: stop or pause that instance, not every one of
  the `Sound`.

## After 1.0

- Input and window: gamepads, the clipboard, files dropped on the window,
  monitors, the window's position, icon and size limits.
- Text: TTF fonts, letter spacing, wrapping to a width.
- Images on the CPU: crop, resize, flip, draw into, generate (gradients,
  checks, noise).
- Audio: pitch and pan for `Music`, sound generated on the fly.
- Shapes: splines and Bezier curves, ellipses.
- Collisions: rays with the distance to the hit, moving rectangles (swept),
  rotated rectangles and convex polygons (SAT).
- Math: `reflect`, the angle between vectors, `Mat4` inverse and transpose.
- Build: `BUILD_SHARED_LIBS` (an export macro on the public API, hidden
  symbols of the bundled libraries, a SOVERSION), clang-cl on Windows, a
  pkg-config file for builds without CMake.
- Loading from memory: `Texture::decode`, `Music::decode`.

## Deferred

Looked at and left until there is a need:

- Header weight. `<format>` (through the asserts) sets a floor every header
  pays, and programs include `tgx/tgx.h` anyway, so splitting headers buys
  little.
- `FrameClock` out of the GL `Context`. Cleaner, but it only pays off with a
  public state reset, which nothing asks for.
- NaN checks in release builds. The asserts are the contract, as for an index
  out of range; checking again without them would double them.

## Not planned

Decided against:

- 3D on the simple level before 1.0: 3D stays on the OpenGL level.
- Exclusive fullscreen: impossible on Wayland; borderless at the desktop
  resolution covers the use.
- Threads inside tgx: GL and the window belong to the main thread; loading on
  other threads is the app's, and `Sound` already allows it.
- Event bus, ECS, scenes: engine level, not a library's.
- Touch, gestures, VR: not the platforms tgx targets.
- Begin/end pairs in the API: every call stands on its own.
