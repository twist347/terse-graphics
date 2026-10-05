#pragma once

// The simple level: a window, a loop, and 2D drawing on the Canvas. Normal use
// needs nothing from OpenGL; tgx/gl.h adds the level underneath, for drawing
// with your own shaders and buffers.

#include "tgx/app.h"
#include "tgx/assert.h"
#include "tgx/audio.h"
#include "tgx/blend.h"
#include "tgx/camera.h"
#include "tgx/canvas.h"
#include "tgx/clock.h"
#include "tgx/collision.h"
#include "tgx/color.h"
#include "tgx/device.h"
#include "tgx/error.h"
#include "tgx/image.h"
#include "tgx/input.h"
#include "tgx/log.h"
#include "tgx/math.h"
#include "tgx/render_target.h"
#include "tgx/texture.h"
#include "tgx/window.h"
