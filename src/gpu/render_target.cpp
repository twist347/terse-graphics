#include "tgx/core/render_target.hpp"

#include "gpu/context.hpp"
#include "gpu/gl_error.hpp"

#include <glad/gl.h>

#include <optional>
#include <utility>

namespace {
    // Depth only: nothing in tgx uses a stencil.
    [[nodiscard]] auto make_depth(tgx::Size size) noexcept -> tgx::Result<GLuint> {
        tgx::detail::drain_gl_errors();

        GLuint id = 0;
        glGenRenderbuffers(1, &id);
        glBindRenderbuffer(GL_RENDERBUFFER, id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.width, size.height);

        return tgx::detail::checked_allocation<tgx::detail::delete_renderbuffer>(id);
    }
}

namespace tgx {
    auto detail::delete_framebuffer(GlId id) noexcept -> void {
        detail::context().before_delete_framebuffer(id);
        glDeleteFramebuffers(1, &id);
    }

    auto detail::delete_renderbuffer(GlId id) noexcept -> void {
        glDeleteRenderbuffers(1, &id);
    }

    auto RenderTarget::create(Size size, const RenderTargetParams &params) noexcept -> Result<RenderTarget> {
        // Filled only by draws: no mipmaps to rebuild, no CPU updates (their
        // rows go the other way).
        auto texture = Texture::create_for_target(
            size,
            {.filter = params.filter, .wrap = params.wrap, .mipmaps = false, .access = TextureAccess::immutable}
        );
        if (!texture) {
            return std::unexpected{texture.error()};
        }

        detail::Handle<detail::delete_renderbuffer> depth{};
        if (params.depth) {
            auto id = make_depth(size);
            if (!id) {
                return std::unexpected{id.error()};
            }
            depth = detail::Handle<detail::delete_renderbuffer>{*id};
        }

        detail::drain_gl_errors();

        GLuint id = 0;
        glGenFramebuffers(1, &id);
        if (const auto generated = detail::checked_allocation<detail::delete_framebuffer>(id); !generated) {
            return std::unexpected{generated.error()};
        }
        // Framebuffer 0 is the window, not a failed render target.
        if (id == 0) {
            return std::unexpected{Error::platform};
        }

        detail::context().bind_framebuffer(id);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id(), 0);
        if (depth) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth.get());
        }
        if (const auto attached = detail::checked_allocation<detail::delete_framebuffer>(id); !attached) {
            return std::unexpected{attached.error()};
        }
        detail::Handle<detail::delete_framebuffer> framebuffer{id};

        // GL 3.3 may refuse a combination of formats it does not draw into.
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            return std::unexpected{Error::unsupported};
        }

        // New contents are undefined; a depth test into an uncleared buffer
        // may show nothing (Mesa).
        detail::context().clear({
            .target = {id, texture->id(), size, static_cast<bool>(depth)},
            .color = colors::transparent,
            .depth = depth ? std::optional{1.f} : std::nullopt,
        });

        return RenderTarget{std::move(*texture), std::move(depth), std::move(framebuffer)};
    }

    auto RenderTarget::read() const -> Image {
        return detail::context().read(detail::target_of(this));
    }
}
