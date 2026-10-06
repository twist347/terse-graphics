#include "tgx/render_target.h"

#include "context.h"
#include "gl_error.h"

#include <glad/gl.h>

#include <utility>

namespace {
    // Depth only: nothing in tgx uses a stencil.
    [[nodiscard]] auto make_depth(tgx::Size size) noexcept -> tgx::Result<GLuint> {
        tgx::detail::drain_gl_errors();

        GLuint id = 0;
        glGenRenderbuffers(1, &id);
        glBindRenderbuffer(GL_RENDERBUFFER, id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.width, size.height);

        if (const GLenum err = glGetError(); err != GL_NO_ERROR) {
            tgx::detail::delete_renderbuffer(id);
            return std::unexpected{tgx::detail::to_error(err)};
        }
        return id;
    }
}

namespace tgx {
    auto detail::delete_framebuffer(GlId id) noexcept -> void {
        // Shapes added before are drawn into it while it is still there.
        detail::flush_target_use(id);
        detail::context().forget_framebuffer(id);
        glDeleteFramebuffers(1, &id);
    }

    auto detail::delete_renderbuffer(GlId id) noexcept -> void {
        glDeleteRenderbuffers(1, &id);
    }

    auto RenderTarget::create(Size size, const RenderTargetParams &params) noexcept -> Result<RenderTarget> {
        // Its contents come from draws only: no mipmaps to rebuild after each,
        // and no updates from the CPU, whose rows run the other way.
        auto texture = Texture::create_for_target(
            size,
            {.filter = params.filter, .wrap = params.wrap, .mipmaps = false, .access = TextureAccess::immutable}
        );
        if (!texture) {
            return std::unexpected{texture.error()};
        }

        detail::Handle<detail::delete_renderbuffer> depth;
        if (params.depth) {
            auto id = make_depth(size);
            if (!id) {
                return std::unexpected{id.error()};
            }
            depth = detail::Handle<detail::delete_renderbuffer>{*id};
        }

        GLuint id = 0;
        glGenFramebuffers(1, &id);
        detail::Handle<detail::delete_framebuffer> framebuffer{id};

        detail::context().bind_framebuffer(id);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id(), 0);
        if (depth) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth.get());
        }

        // GL 3.3 may refuse a combination of formats it does not draw into.
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            return std::unexpected{Error::unsupported};
        }

        return RenderTarget{std::move(*texture), std::move(depth), std::move(framebuffer)};
    }

    auto RenderTarget::read() const -> Image {
        return detail::context().read(detail::target_of(this));
    }
}
