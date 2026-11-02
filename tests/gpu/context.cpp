// The window's actual depth capability, including an absent attachment.

#include "gpu.hpp"

#include "gpu/context.hpp"

#include <doctest/doctest.h>
#include <glad/gl.h>

#include <utility>

namespace {
    // Reports a synthetic depth attachment; the actual framebuffer stays as is.
    struct DepthQuery {
        explicit DepthQuery(int bits)
            : bits{bits}, original{std::exchange(glad_glGetFramebufferAttachmentParameteriv, query)} {
            current = this;
        }
        DepthQuery(const DepthQuery &) = delete;
        auto operator=(const DepthQuery &) -> DepthQuery & = delete;
        ~DepthQuery() {
            glad_glGetFramebufferAttachmentParameteriv = original;
            current = nullptr;
        }

        int bits;
        int size_queries{0};
        bool default_bound{true};

    private:
        static void GLAD_API_PTR query(GLenum target, GLenum attachment, GLenum name, GLint *value) {
            if (target == GL_FRAMEBUFFER && attachment == GL_DEPTH) {
                GLint framebuffer = -1;
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
                current->default_bound = current->default_bound && framebuffer == 0;
                if (name == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) {
                    *value = current->bits == 0 ? GL_NONE : GL_FRAMEBUFFER_DEFAULT;
                    return;
                }
                if (name == GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE) {
                    ++current->size_queries;
                    *value = current->bits;
                    return;
                }
            }
            current->original(target, attachment, name, value);
        }

        static inline DepthQuery *current{nullptr};
        PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC original;
    };
}

TEST_CASE("Context queries the window's depth and skips the size of an absent attachment") {
    auto &context = tgx::detail::context();
    context.flush();
    context.bind_framebuffer(0);
    GLint type = GL_NONE;
    GLint bits = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    if (type != GL_NONE) {
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &bits);
    }
    CHECK(context.window_depth_bits == bits);
    CHECK(context.has_depth({}) == (bits > 0));
    if (bits > 0) {
        const auto cover = tgx_test::Cover::create();
        tgx_test::app().device().clear({.depth = 1.f});
        cover.draw({.state = {.depth = tgx::gl::Depth::less}});
        CHECK(glGetError() == GL_NO_ERROR);
    }

    for (const int reported: {0, 16, 24}) {
        CAPTURE(reported);
        context.close();
        {
            DepthQuery query{reported};
            const auto opened = context.open();
            CHECK(opened.has_value());
            if (opened) {
                CHECK(context.window_depth_bits == reported);
                CHECK(context.has_depth({}) == (reported > 0));
                CHECK(query.size_queries == (reported > 0 ? 1 : 0));
                CHECK(query.default_bound);
                // 2D drawing still works when no depth was reported.
                auto canvas = tgx::Canvas::create();
                canvas.rect({0, 0, 4, 4}, tgx::colors::red);
                context.flush();
                CHECK(glGetError() == GL_NO_ERROR);
            }
            context.close();
        }
        // Restore real capabilities before the next test uses the shared App.
        REQUIRE(context.open().has_value());
        CHECK(context.window_depth_bits == bits);
    }
}
