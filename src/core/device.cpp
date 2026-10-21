#include "tgx/core/device.hpp"

#include "tgx/core/assert.hpp"
#include "tgx/core/render_target.hpp"
#include "tgx/core/texture.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/shader.hpp"
#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/version.hpp"
#include "tgx/gl/vertex_array.hpp"

#include "core/batch.hpp"
#include "core/context.hpp"
#include "core/gl_debug.hpp"
#include "core/window_internal.hpp"
#include "gl/vertex_array_internal.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

// Only the 3.3 backend exists; this is not a version switch.
static_assert(
    tgx::gl::version_major == 3 && tgx::gl::version_minor == 3,
    "tgx implements only OpenGL 3.3"
);

namespace {
    // Asserts each sampler has a texture in its slot. Otherwise it reads what
    // an earlier draw left, or black, and GL reports neither. Unused with
    // asserts off.
    [[maybe_unused]] auto check_textures(
        const tgx::gl::Shader &shader,
        const tgx::gl::DrawParams &params
    ) noexcept -> void {
        for (const auto &sampler: tgx::gl::detail::samplers(shader)) {
            // From GL, not tracked: the canvas and layout(binding) set it too.
            GLint slot = 0;
            glGetUniformiv(shader.id(), sampler.location, &slot);
            const auto index = static_cast<std::size_t>(slot);
            TGX_ASSERT_MSG(
                index < params.textures.size() && params.textures[index],
                "sampler '{}' reads texture slot {}, which the draw has no texture for",
                sampler.name, slot
            );
        }
    }
}

namespace tgx {
    auto Device::create(Window &) -> Result<Device> {
        // glad 2 returns the version it loaded.
        const int version = gladLoadGL(detail::gl_loader());
        if (version == 0) {
            return std::unexpected{Error::platform};
        }
        // Before the version check, so a rejected context is still logged.
        detail::log_context_info();
        if (version < GLAD_MAKE_VERSION(gl::version_major, gl::version_minor)) {
            return std::unexpected{Error::unsupported};
        }

        detail::install_debug_callback();

        detail::Context &context = detail::context();
        context.reset();

        auto batch = detail::Batch::create();
        if (!batch) {
            context = {};
            return std::unexpected{batch.error()};
        }
        context.batch = std::make_unique<detail::Batch>(std::move(*batch));

        // One blank frame now: on Wayland with a fractional scale, the buffer
        // gets the size GLFW reports only at a swap, so the first frame would
        // be cut off and flipped with the wrong height. Not present(): it is
        // not an app frame, and the clock does not tick.
        context.clear({.color = colors::black});
        detail::swap_buffers();
        context.frame = {};
        return Device{};
    }

    auto Device::clear(const ClearParams &params) noexcept -> void {
        const detail::Target target = detail::target_of(params.target);
        // GL silently ignores a clear of a missing buffer.
        TGX_ASSERT_MSG(
            !params.depth || target.depth,
            "clearing the depth of a render target made without one"
        );

        detail::context().clear({
            .target = target,
            .color = params.color,
            .depth = params.depth,
        });
    }

    auto Device::flush() noexcept -> void {
        detail::context().flush();
    }

    auto Device::read() -> Image {
        Image image = detail::context().read({});
        // The window's alpha is whatever blending left, but it shows opaque.
        for (Color &pixel: image.pixels()) {
            pixel.a = 255;
        }
        return image;
    }

    auto Device::present() noexcept -> void {
        detail::Context &context = detail::context();
        context.flush();
        detail::swap_buffers();
        context.clock.tick();
        context.last_frame = std::exchange(context.frame, {});
    }

    auto Device::clock() const noexcept -> const Clock & {
        static constexpr Clock clock{};
        return clock;
    }

    auto Device::restart_clock() noexcept -> void {
        detail::context().clock.restart();
    }

    auto Device::stats() const noexcept -> FrameStats {
        return detail::context().last_frame;
    }

    auto Device::draw(const gl::Shader &shader, const gl::VertexArray &vertices) noexcept -> void {
        draw(shader, vertices, {});
    }

    auto Device::draw(
        const gl::Shader &shader,
        const gl::VertexArray &vertices,
        const gl::DrawParams &params
    ) noexcept -> void {
        TGX_ASSERT_MSG(vertices.vertex_count() > 0, "drawing from a vertex array with no vertex buffer");
        if constexpr (TGX_ENABLE_ASSERTS != 0) {
            gl::detail::check_vertex_inputs(shader, vertices);
            check_textures(shader, params);
        }

        const bool indexed = vertices.has_index_buffer();
        const std::size_t available = indexed ? vertices.index_count() : vertices.vertex_count();
        const std::size_t count = params.count != gl::DrawParams::all
                                      ? params.count
                                      : available - std::min(params.first, available);

        TGX_ASSERT(std::in_range<GLsizei>(count) && std::in_range<GLint>(params.first));

        // GL does not check ranges: reading past a buffer is undefined and
        // usually silent. Index values are not checked; that would need a read
        // back. Both values fit a GLint, so the sum cannot overflow.
        TGX_ASSERT_MSG(
            params.first + count <= available,
            "drawing {} [{}, {}) from a buffer of {}",
            indexed ? "indices" : "vertices", params.first, params.first + count, available
        );

        const detail::Target target = detail::target_of(params.target);
        // Without a depth buffer every fragment passes, silently.
        TGX_ASSERT_MSG(
            params.state.depth == gl::Depth::none || target.depth,
            "a depth test drawing into a render target made without depth"
        );
        const gl::Viewport viewport = params.viewport.value_or(detail::surface_of(target).viewport());
        // GL draws nothing, and reports it only on a debug context.
        TGX_ASSERT_MSG(
            viewport.width >= 0 && viewport.height >= 0,
            "viewport of size {}x{}",
            viewport.width, viewport.height
        );
        detail::DrawCall call{
            .target = target,
            .program = shader.id(),
            .vertex_array = vertices.id(),
            .index_type = indexed ? std::optional{vertices.index_type()} : std::nullopt,
            .first = params.first,
            .count = count,
            .primitive = params.primitive,
            .state = params.state,
            .viewport = viewport,
        };
        for (std::uint32_t slot = 0; slot < gl::max_texture_slots; ++slot) {
            if (const Texture *texture = params.textures[slot]) {
                // Undefined in GL; drivers draw garbage.
                TGX_ASSERT_MSG(
                    texture->id() != target.texture,
                    "slot {} reads the texture of the render target the draw writes",
                    slot
                );
                call.textures[slot] = texture->id();
            }
        }
        detail::context().draw(call);
    }

    auto detail::release_device() noexcept -> void {
        // The batch goes while GL and the Context are still whole. Its shapes
        // are dropped: their frame is over.
        Context &state = context();
        state.batch.reset();
        state = {};
    }
}
