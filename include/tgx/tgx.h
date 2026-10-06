#pragma once

// tgx, whole: the one header a program includes. The other headers under
// tgx/ are how it is put together, not entry points; their paths may change.
//
// Two levels. tgx:: is a window, a loop, and 2D drawing on the Canvas, with
// images, textures, sound and the math and checks games need; normal use
// needs nothing more. tgx::gl:: is the OpenGL level underneath, for drawing
// with your own shaders and buffers: the Device's draw calls and the raw
// resources they take, mixing with the Canvas in the same frame.

#include "tgx/core/app.h"
#include "tgx/core/assert.h"
#include "tgx/core/audio.h"
#include "tgx/core/blend.h"
#include "tgx/core/camera.h"
#include "tgx/core/canvas.h"
#include "tgx/core/clock.h"
#include "tgx/core/collision.h"
#include "tgx/core/color.h"
#include "tgx/core/device.h"
#include "tgx/core/error.h"
#include "tgx/core/image.h"
#include "tgx/core/input.h"
#include "tgx/core/log.h"
#include "tgx/core/math.h"
#include "tgx/core/random.h"
#include "tgx/core/render_target.h"
#include "tgx/core/texture.h"
#include "tgx/core/window.h"

#include "tgx/gl/buffer.h"
#include "tgx/gl/draw.h"
#include "tgx/gl/shader.h"
#include "tgx/gl/texture_slot.h"
#include "tgx/gl/version.h"
#include "tgx/gl/vertex_array.h"
