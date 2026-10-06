#pragma once

#include "tgx/core/error.h"
#include "tgx/core/handle.h"
#include "tgx/core/image.h"
#include "tgx/core/math.h"
#include "tgx/core/texture.h"

#include <utility>

namespace tgx {
    namespace detail {
        auto delete_framebuffer(GlId id) noexcept -> void;
        auto delete_renderbuffer(GlId id) noexcept -> void;
    }

    struct RenderTargetParams {
        // How its texture is read when drawn larger or smaller: nearest keeps
        // pixel art square.
        TextureFilter filter{TextureFilter::linear};
        TextureWrap wrap{TextureWrap::clamp};
        // A depth buffer, for draws with a depth test (gl::Depth). 2D drawing
        // needs none.
        bool depth{false};
    };

    // A texture to draw into instead of the window: the Canvas with
    // set_target(), a draw of your own with DrawParams::target. Then drawn
    // like any texture, e.g. as a sprite scaled up for pixel art. Its
    // coordinates are its pixels, from the top-left.
    //
    //     // pixels: RenderTarget::create({320, 180}, {.filter = TextureFilter::nearest})
    //     tgx::Canvas world = app->canvas();
    //     world.set_target(&pixels);
    //     ... draw the world ...
    //     app->canvas().sprite(pixels.texture(), {.size = {1280, 720}});
    //
    // Its see-through parts hold premultiplied colors, as Blend::alpha leaves
    // them: draw it with Blend::premultiplied, or a half see-through white
    // comes out a quarter gray. Opaque parts look the same either way.
    //
    // Lives inside the Device: created after it, destroyed before it. Shapes
    // still waiting to be drawn into it are drawn first when it goes.
    class RenderTarget {
    public:
        // Of size pixels, which must not be empty. Fails as Texture::create
        // does, and with Error::unsupported when the driver cannot draw into
        // such a texture.
        [[nodiscard]] static auto create(
            Size size,
            const RenderTargetParams &params = {}
        ) noexcept -> Result<RenderTarget>;

        // What has been drawn into it. Its rows are bottom to top, as GL draws
        // (Texture::bottom_up()): the Canvas takes care of that, a shader of
        // your own reads v = 0 as its bottom.
        [[nodiscard]] auto texture() const noexcept -> const Texture & { return m_texture; }

        [[nodiscard]] auto size() const noexcept -> Size { return m_texture.size(); }

        // Whether it was made with a depth buffer (RenderTargetParams::depth):
        // what a draw with a depth test, or a clear of depth, needs.
        [[nodiscard]] auto has_depth() const noexcept -> bool { return static_cast<bool>(m_depth); }

        // The GL framebuffer: the way out to raw GL.
        [[nodiscard]] auto id() const noexcept -> GlId { return m_framebuffer.get(); }

        // Its pixels back on the CPU, rows top to bottom like any Image. Draws
        // what is waiting first, and waits for the GPU to finish: for a
        // screenshot or a test, not every frame. See-through pixels come
        // premultiplied, unlike those of a loaded image.
        [[nodiscard]] auto read() const -> Image;

    private:
        RenderTarget(
            Texture texture,
            detail::Handle<detail::delete_renderbuffer> depth,
            detail::Handle<detail::delete_framebuffer> framebuffer
        ) noexcept
            : m_texture{std::move(texture)}, m_depth{std::move(depth)}, m_framebuffer{std::move(framebuffer)} {
        }

        Texture m_texture;
        detail::Handle<detail::delete_renderbuffer> m_depth;
        detail::Handle<detail::delete_framebuffer> m_framebuffer;
    };
}

namespace tgx::detail {
    // What a draw goes into, by id: framebuffer 0 is the window. A render
    // target's size is fixed, so it travels with the id; the window's is read
    // when it is drawn into, as it can change in between. The texture tells
    // the batch to draw shapes into the target before its texture changes or
    // goes, and asserts that a draw does not read what it writes; the depth
    // asserts that a depth test has something to test against.
    struct Target {
        GlId framebuffer{0};
        GlId texture{0};
        Size size{};
        // The window is created with one (Window::create asks for it).
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
