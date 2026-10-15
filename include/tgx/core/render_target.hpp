#pragma once

#include "tgx/core/error.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/image.hpp"
#include "tgx/core/math.hpp"
#include "tgx/core/texture.hpp"

#include <utility>

namespace tgx {
    namespace detail {
        auto delete_framebuffer(GlId id) noexcept -> void;
        auto delete_renderbuffer(GlId id) noexcept -> void;
    }

    struct RenderTargetParams {
        // nearest keeps pixel art square.
        TextureFilter filter{TextureFilter::linear};
        TextureWrap wrap{TextureWrap::clamp};
        // A depth buffer, for draws with a depth test (gl::Depth). 2D drawing
        // does not need one.
        bool depth{false};
    };

    // A texture to draw into instead of the window, with Canvas::set_target()
    // or DrawParams::target, then draw like any texture. Coordinates are its
    // pixels, from the top-left.
    //
    //     // pixels: RenderTarget::create({320, 180}, {.filter = TextureFilter::nearest})
    //     tgx::Canvas world = app->canvas();
    //     world.set_target(&pixels);
    //     ... draw the world ...
    //     app->canvas().sprite(pixels.texture(), {.size = {1280, 720}});
    //
    // Its colors are premultiplied, as Blend::alpha leaves them: draw it with
    // Blend::premultiplied, or half transparent white comes out a quarter
    // gray. It starts transparent; a clear premultiplies its color, but
    // Blend::none does not (use Color::premultiplied()).
    //
    // Create it after the Device and destroy it before the Device. Waiting
    // shapes are drawn into it before it is destroyed.
    class RenderTarget {
    public:
        // size must not be empty. Fails like Texture::create, and with
        // Error::unsupported when the driver cannot draw into it.
        [[nodiscard]] static auto create(
            Size size,
            const RenderTargetParams &params = {}
        ) noexcept -> Result<RenderTarget>;

        RenderTarget(const RenderTarget &) = delete;
        auto operator=(const RenderTarget &) -> RenderTarget & = delete;

        RenderTarget(RenderTarget &&) noexcept = default;
        auto operator=(RenderTarget &&) noexcept -> RenderTarget & = default;

        // Rows go bottom to top (Texture::bottom_up()). The Canvas handles
        // that; in your own shader v = 0 is the bottom.
        [[nodiscard]] auto texture() const noexcept -> const Texture & { return texture_; }

        [[nodiscard]] auto size() const noexcept -> Size { return texture_.size(); }

        // Needed by a draw with a depth test or a clear of depth.
        [[nodiscard]] auto has_depth() const noexcept -> bool { return static_cast<bool>(depth_); }

        // The GL framebuffer id.
        [[nodiscard]] auto id() const noexcept -> GlId { return framebuffer_.get(); }

        // Pixels back to the CPU, rows top to bottom. Draws the waiting shapes
        // and waits for the GPU, so not every frame. Colors stay premultiplied.
        [[nodiscard]] auto read() const -> Image;

    private:
        RenderTarget(
            Texture texture,
            detail::Handle<detail::delete_renderbuffer> depth,
            detail::Handle<detail::delete_framebuffer> framebuffer
        ) noexcept
            : texture_{std::move(texture)}, depth_{std::move(depth)}, framebuffer_{std::move(framebuffer)} {
        }

        Texture texture_;
        detail::Handle<detail::delete_renderbuffer> depth_;
        detail::Handle<detail::delete_framebuffer> framebuffer_;
    };
}

namespace tgx::detail {
    // Where a draw goes; framebuffer 0 is the window, sized at draw time.
    // texture and depth let the batch flush before the texture changes and
    // assert that a draw does not read what it writes or depth-test without
    // a depth buffer.
    struct Target {
        GlId framebuffer{0};
        GlId texture{0};
        Size size{};
        // Window::create asks for one.
        bool depth{true};

        [[nodiscard]] constexpr auto operator==(const Target &) const noexcept -> bool = default;
    };

    // nullptr is the window.
    [[nodiscard]] inline auto target_of(const RenderTarget *target) noexcept -> Target {
        if (!target) {
            return {};
        }
        return {target->id(), target->texture().id(), target->size(), target->has_depth()};
    }
}
