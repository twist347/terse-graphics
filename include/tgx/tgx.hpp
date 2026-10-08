#pragma once

// tgx, whole: the one header a program includes. The other headers under
// tgx/ are how it is put together, not entry points; their paths may change.
//
// Two levels. tgx:: is a window, a loop, and 2D drawing on the Canvas, with
// images, textures, sound and the math and checks games need; normal use
// needs nothing more. tgx::gl:: is the OpenGL level underneath, for drawing
// with your own shaders and buffers: the Device's draw calls and the raw
// resources they take, mixing with the Canvas in the same frame.

#include "tgx/core/app.hpp"
#include "tgx/core/assert.hpp"
#include "tgx/core/audio.hpp"
#include "tgx/core/blend.hpp"
#include "tgx/core/camera.hpp"
#include "tgx/core/canvas.hpp"
#include "tgx/core/clock.hpp"
#include "tgx/core/collision.hpp"
#include "tgx/core/color.hpp"
#include "tgx/core/device.hpp"
#include "tgx/core/error.hpp"
#include "tgx/core/image.hpp"
#include "tgx/core/input.hpp"
#include "tgx/core/log.hpp"
#include "tgx/core/math.hpp"
#include "tgx/core/random.hpp"
#include "tgx/core/render_target.hpp"
#include "tgx/core/texture.hpp"
#include "tgx/core/version.hpp"
#include "tgx/core/window.hpp"

#include "tgx/gl/buffer.hpp"
#include "tgx/gl/draw.hpp"
#include "tgx/gl/shader.hpp"
#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/version.hpp"
#include "tgx/gl/vertex_array.hpp"
