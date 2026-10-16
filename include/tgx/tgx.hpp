#pragma once

// All of tgx, in the one header to include. The other headers' paths may
// change.
//
// Two levels. tgx:: gives a window, a loop, 2D drawing on the Canvas,
// images, textures, sound, math and checks: all normal use needs.
// tgx::gl:: is the OpenGL level below, for your own shaders and buffers:
// the Device's draw calls and their resources. It mixes with the Canvas in
// the same frame.

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
#include "tgx/core/ease.hpp"
#include "tgx/core/error.hpp"
#include "tgx/core/image.hpp"
#include "tgx/core/input.hpp"
#include "tgx/core/log.hpp"
#include "tgx/core/math.hpp"
#include "tgx/core/paths.hpp"
#include "tgx/core/random.hpp"
#include "tgx/core/render_target.hpp"
#include "tgx/core/texture.hpp"
#include "tgx/core/timer.hpp"
#include "tgx/core/version.hpp"
#include "tgx/core/window.hpp"

#include "tgx/gl/buffer.hpp"
#include "tgx/gl/draw.hpp"
#include "tgx/gl/shader.hpp"
#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/version.hpp"
#include "tgx/gl/vertex_array.hpp"
