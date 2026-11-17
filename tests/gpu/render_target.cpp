// Render targets: drawn into, read back, drawn themselves, and going while
// shapes for them wait.

#include "gpu.hpp"

#include <doctest/doctest.h>
#include <glad/gl.h>

#include <array>
#include <initializer_list>
#include <utility>

namespace {
    // A rejected bind leaves the current framebuffer and its attachments
    // intact. Inject once, so later binds can test cache recovery too.
    class FramebufferBindFailure {
    public:
        explicit FramebufferBindFailure(GLenum error) noexcept
            : error_{error}, bind_{std::exchange(glad_glBindFramebuffer, bind)},
              attach_texture_{std::exchange(glad_glFramebufferTexture2D, attach_texture)},
              attach_depth_{std::exchange(glad_glFramebufferRenderbuffer, attach_depth)},
              delete_{std::exchange(glad_glDeleteFramebuffers, delete_framebuffers)},
              get_error_{std::exchange(glad_glGetError, get_error)} {
            current_ = this;
        }
        FramebufferBindFailure(const FramebufferBindFailure &) = delete;
        auto operator=(const FramebufferBindFailure &) -> FramebufferBindFailure & = delete;
        ~FramebufferBindFailure() {
            glad_glBindFramebuffer = bind_;
            glad_glFramebufferTexture2D = attach_texture_;
            glad_glFramebufferRenderbuffer = attach_depth_;
            glad_glDeleteFramebuffers = delete_;
            glad_glGetError = get_error_;
            current_ = nullptr;
        }

        GLuint rejected{0};
        bool deleted{false};
        int attachments{0};

    private:
        static void GLAD_API_PTR bind(GLenum target, GLuint id) {
            if (current_->rejected == 0 && id != 0) {
                current_->rejected = id;
                current_->pending_ = true;
                return;
            }
            current_->bind_(target, id);
        }
        static void GLAD_API_PTR attach_texture(GLenum target, GLenum attachment, GLenum type, GLuint id, GLint level) {
            ++current_->attachments;
            current_->attach_texture_(target, attachment, type, id, level);
        }
        static void GLAD_API_PTR attach_depth(GLenum target, GLenum attachment, GLenum type, GLuint id) {
            ++current_->attachments;
            current_->attach_depth_(target, attachment, type, id);
        }
        static void GLAD_API_PTR delete_framebuffers(GLsizei count, const GLuint *ids) {
            for (GLsizei i = 0; i < count; ++i) {
                current_->deleted |= ids[i] == current_->rejected;
            }
            current_->delete_(count, ids);
        }
        static auto GLAD_API_PTR get_error() -> GLenum {
            return std::exchange(current_->pending_, false) ? current_->error_ : current_->get_error_();
        }

        static inline FramebufferBindFailure *current_{nullptr};
        GLenum error_;
        bool pending_{false};
        PFNGLBINDFRAMEBUFFERPROC bind_;
        PFNGLFRAMEBUFFERTEXTURE2DPROC attach_texture_;
        PFNGLFRAMEBUFFERRENDERBUFFERPROC attach_depth_;
        PFNGLDELETEFRAMEBUFFERSPROC delete_;
        PFNGLGETERRORPROC get_error_;
    };

    // Red top-left quarter on blue.
    auto draw_quarter(tgx::Canvas &canvas) -> void {
        canvas.clear(tgx::colors::blue);
        canvas.rect({0, 0, 32, 16}, tgx::colors::red);
    }
}

TEST_CASE("a rejected framebuffer bind preserves the previous target and cached binding") {
    const auto cover = tgx_test::Cover::create();
    auto &device = tgx_test::app().device();
    for (const GLenum error: std::array<GLenum, 2>{GL_OUT_OF_MEMORY, GL_INVALID_OPERATION}) {
        for (const bool depth: {false, true}) {
            CAPTURE(error);
            CAPTURE(depth);
            const auto previous = tgx_test::target({8, 8}, true);
            device.clear({.target = &previous, .color = tgx::colors::red, .depth = 1.f});
            GLint old_depth = 0;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &old_depth);
            {
                FramebufferBindFailure failure{error};
                const auto result = tgx::RenderTarget::create({8, 8}, {.depth = depth});
                REQUIRE_FALSE(result.has_value());
                CHECK(result.error() == (error == GL_OUT_OF_MEMORY ? tgx::Error::out_of_memory : tgx::Error::platform));
                CHECK(failure.rejected != 0);
                CHECK(failure.deleted);
                CHECK(failure.attachments == 0);
            }
            CHECK(glGetError() == GL_NO_ERROR);
            GLint binding = 0;
            GLint color = 0;
            GLint depth_attachment = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &binding);
            CHECK(static_cast<tgx::GlId>(binding) == previous.id());
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &color);
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depth_attachment);
            REQUIRE(static_cast<tgx::GlId>(color) == previous.texture().id());
            REQUIRE(depth_attachment == old_depth);

            // If cleanup forgot a bind that GL rejected, the cache would say
            // 0 while the old target was still bound. This clear must switch
            // to the window rather than overwrite the old target.
            device.clear({.color = tgx::colors::blue});
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &binding);
            CHECK(binding == 0);
            CHECK(previous.read()[4, 4] == tgx::colors::red);
            cover.draw({.target = &previous, .state = {.depth = tgx::gl::Depth::less}});
            CHECK(previous.read()[4, 4] == tgx::colors::green);

            const auto after = tgx_test::target({8, 8}, depth);
            cover.draw({.target = &after});
            CHECK(after.read()[4, 4] == tgx::colors::green);
            CHECK(glGetError() == GL_NO_ERROR);
        }
    }
}

TEST_CASE("a canvas on a target spans its pixels, read back rows top to bottom") {
    const tgx::RenderTarget target = tgx_test::target({64, 32});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    CHECK(canvas.size() == tgx::Size{64, 32});

    draw_quarter(canvas);

    const tgx::Image image = target.read();
    CHECK(image.size() == tgx::Size{64, 32});
    CHECK(image[0, 0] == tgx::colors::red);
    CHECK(image[31, 15] == tgx::colors::red);
    CHECK(image[32, 0] == tgx::colors::blue);
    CHECK(image[0, 16] == tgx::colors::blue);
    CHECK(image[63, 31] == tgx::colors::blue);
}

TEST_CASE("a target drawn as a sprite: the right way up, mirrored, a part of it") {
    const tgx::RenderTarget target = tgx_test::target({64, 32});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    draw_quarter(canvas);
    const tgx::RenderTarget out = tgx_test::target({64, 64});
    tgx::Canvas screen = tgx_test::canvas_on(out);

    screen.clear(tgx::colors::black);
    screen.sprite(target.texture(), {.size = {64, 32}});
    tgx::Image image = out.read();
    CHECK(image[2, 2] == tgx::colors::red);
    CHECK(image[2, 28] == tgx::colors::blue);
    CHECK(image[60, 2] == tgx::colors::blue);
    CHECK(image[32, 48] == tgx::colors::black);

    screen.clear(tgx::colors::black);
    screen.sprite(target.texture(), {.size = {64, 32}, .src = {0, 0, 64, -32}});
    image = out.read();
    CHECK(image[2, 28] == tgx::colors::red);
    CHECK(image[2, 2] == tgx::colors::blue);

    screen.clear(tgx::colors::black);
    screen.sprite(target.texture(), {.size = {32, 32}, .src = {0, 0, 32, 16}});
    image = out.read();
    CHECK(image[16, 30] == tgx::colors::red);
    CHECK(image[40, 16] == tgx::colors::black);
}

TEST_CASE("a canvas with a viewport on a target, and to_world in its pixels") {
    const tgx::RenderTarget target = tgx_test::target({64, 32});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    draw_quarter(canvas);

    tgx::Canvas quarter = canvas;
    quarter.set_viewport(tgx::Rect{32, 16, 32, 16});
    CHECK(quarter.size() == tgx::Size{32, 16});
    quarter.rect({0, 0, 32, 16}, tgx::colors::yellow);

    const tgx::Image image = target.read();
    CHECK(image[40, 20] == tgx::colors::yellow);
    CHECK(image[0, 0] == tgx::colors::red);
    CHECK(image[40, 4] == tgx::colors::blue);

    tgx::Canvas small = canvas;
    small.set_size({16, 8});
    CHECK(small.to_world({32, 16}) == tgx::Vec2{8, 4});
}

TEST_CASE("draws into two targets keep the call order") {
    const tgx::RenderTarget target = tgx_test::target({64, 32});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const tgx::RenderTarget out = tgx_test::target({64, 64});
    tgx::Canvas screen = tgx_test::canvas_on(out);

    screen.clear(tgx::colors::black);
    screen.rect({0, 0, 64, 64}, tgx::colors::white);
    canvas.clear(tgx::colors::magenta);
    canvas.rect({0, 0, 64, 32}, tgx::colors::cyan);
    screen.sprite(target.texture(), {.size = {64, 32}});
    screen.rect({40, 40, 10, 10}, tgx::colors::red);

    const tgx::Image image = out.read();
    CHECK(image[10, 10] == tgx::colors::cyan);
    CHECK(image[10, 50] == tgx::colors::white);
    CHECK(image[45, 45] == tgx::colors::red);
}

TEST_CASE("a target destroyed while shapes for it wait") {
    {
        const tgx::RenderTarget doomed = tgx_test::target({16, 16});
        tgx::Canvas canvas = tgx_test::canvas_on(doomed);
        canvas.rect({0, 0, 8, 8}, tgx::colors::red);
    }
    const tgx::RenderTarget after = tgx_test::target({16, 16});
    tgx::Canvas canvas = tgx_test::canvas_on(after);
    canvas.clear(tgx::colors::black);
    canvas.rect({0, 0, 4, 4}, tgx::colors::green);

    const tgx::Image image = after.read();
    CHECK(image[1, 1] == tgx::colors::green);
    CHECK(image[10, 10] == tgx::colors::black);
}

TEST_CASE("a target moved and move-assigned while shapes for it wait") {
    tgx::RenderTarget a = tgx_test::target({16, 16});
    tgx::Canvas canvas = tgx_test::canvas_on(a);
    canvas.clear(tgx::colors::black);
    canvas.rect({0, 0, 16, 16}, tgx::colors::red);

    tgx::RenderTarget moved = std::move(a);
    CHECK(moved.read()[8, 8] == tgx::colors::red);

    // The canvas still draws into what moved holds, which goes with blue
    // waiting for it.
    canvas.rect({0, 0, 16, 16}, tgx::colors::blue);
    moved = tgx_test::target({8, 8});
    tgx::Canvas other = tgx_test::canvas_on(moved);
    other.clear(tgx::colors::green);
    CHECK(moved.read()[4, 4] == tgx::colors::green);
}

TEST_CASE("a target with a depth buffer takes a draw with a depth test") {
    const tgx::RenderTarget target = tgx_test::target({32, 32}, true);
    CHECK(target.has_depth());
    const auto cover = tgx_test::Cover::create();

    tgx_test::app().device().clear({.target = &target, .color = tgx::colors::black, .depth = 1.f});
    cover.draw({.target = &target, .state = {.depth = tgx::gl::Depth::less}});
    CHECK(target.read()[5, 5] == tgx::colors::green);
}

TEST_CASE("a target larger than the driver draws into") {
    const auto huge = tgx::RenderTarget::create({100000, 16});
    REQUIRE_FALSE(huge.has_value());
    CHECK(huge.error() == tgx::Error::unsupported);
}

TEST_CASE("see-through parts of a target are premultiplied") {
    const tgx::RenderTarget layer = tgx_test::target({8, 8});
    tgx::Canvas canvas = tgx_test::canvas_on(layer);
    canvas.clear(tgx::colors::transparent);
    canvas.rect({0, 0, 8, 8}, tgx::colors::white.with_alpha(128));
    CHECK(tgx_test::nearly(layer.read()[4, 4], tgx::Color{128, 128, 128, 128}, 1));

    const tgx::RenderTarget out = tgx_test::target({8, 8});
    for (const tgx::Blend blend: {tgx::Blend::alpha, tgx::Blend::premultiplied}) {
        CAPTURE(static_cast<int>(blend));
        tgx::Canvas screen = tgx_test::canvas_on(out);
        screen.clear(tgx::colors::black);
        screen.set_blend(blend);
        screen.sprite(layer.texture(), {.size = {8, 8}});
        // Alpha applies the alpha a second time: a quarter gray, not half.
        const tgx::Color expected = blend == tgx::Blend::alpha ? tgx::Color{64, 64, 64, 255} : tgx::Color{128, 128, 128, 255};
        CHECK(tgx_test::nearly(out.read()[2, 2], expected, 1));
    }
}

TEST_CASE("a new target starts transparent, its depth at the far end") {
    const tgx::RenderTarget target = tgx_test::target({32, 32}, true);
    CHECK(target.read()[5, 5] == tgx::colors::transparent);

    // No clear first: the depth test passes against what create left.
    const auto cover = tgx_test::Cover::create();
    cover.draw({.target = &target, .state = {.depth = tgx::gl::Depth::less}});
    CHECK(target.read()[5, 5] == tgx::colors::green);
}

TEST_CASE("a clear of a target premultiplies its color, as drawing does") {
    const tgx::RenderTarget layer = tgx_test::target({8, 8});
    tgx_test::canvas_on(layer).clear(tgx::colors::white.with_alpha(128));
    CHECK(layer.read()[4, 4] == tgx::Color{128, 128, 128, 128});

    tgx_test::app().device().clear({.target = &layer, .color = tgx::colors::white.with_alpha(128)});
    CHECK(layer.read()[4, 4] == tgx::Color{128, 128, 128, 128});
}

TEST_CASE("a transparent target saved as straight-alpha PNG draws like the original layer") {
    const auto layer = tgx_test::target({8, 8});
    auto canvas = tgx_test::canvas_on(layer);
    canvas.rect({0, 0, 8, 4}, tgx::Color{255, 128, 0, 128});
    auto image = layer.read();
    CHECK(tgx_test::nearly(image[1, 1], {128, 64, 0, 128}, 1));
    CHECK(image[1, 6] == tgx::colors::transparent);

    image.unpremultiply_alpha();
    CHECK(tgx_test::nearly(image[1, 1], {255, 128, 0, 128}, 1));
    const tgx_test::TempFile file{"transparent-layer.png"};
    REQUIRE(image.save(file.path).has_value());
    const auto loaded = tgx_test::required(tgx::Image::load(file.path));
    CHECK(loaded.size() == tgx::Size{8, 8});
    CHECK(loaded[1, 1] == image[1, 1]);
    CHECK(loaded[1, 6] == tgx::colors::transparent);

    const auto restored = tgx_test::texture(loaded);
    const auto out = tgx_test::target({8, 8});
    auto screen = tgx_test::canvas_on(out);
    screen.clear(tgx::colors::blue);
    screen.set_blend(tgx::Blend::premultiplied);
    screen.sprite(layer.texture(), {});
    const auto original = out.read();
    CHECK(tgx_test::nearly(original[1, 1], {128, 64, 127, 255}, 1));

    screen.clear(tgx::colors::blue);
    screen.set_blend(tgx::Blend::alpha);
    screen.sprite(restored, {});
    const auto back = out.read();
    CHECK(tgx_test::nearly(back[1, 1], original[1, 1], 1));
    CHECK(back[1, 6] == tgx::colors::blue);
    CHECK(tgx_test::nearly(layer.read()[1, 1], {128, 64, 0, 128}, 1));
}

TEST_CASE("premultiplied tints scale both RGB and alpha for sprites and nine-slices") {
    const auto layer = tgx_test::target({8, 8});
    auto canvas = tgx_test::canvas_on(layer);
    canvas.rect({0, 0, 8, 4}, tgx::colors::white.with_alpha(128));
    const auto out = tgx_test::target({16, 8});
    auto screen = tgx_test::canvas_on(out);
    screen.set_blend(tgx::Blend::premultiplied);
    const std::array expected{
        tgx::Color{0, 0, 0, 0}, tgx::Color{64, 32, 16, 64}, tgx::Color{128, 64, 32, 128},
    };
    const std::array opacity{0.f, 0.5f, 1.f};
    for (std::size_t i = 0; i < opacity.size(); ++i) {
        CAPTURE(opacity[i]);
        const auto tint = tgx::Color{255, 128, 64, 255}.fade(opacity[i]).premultiplied();
        screen.clear(tgx::colors::transparent);
        screen.sprite(layer.texture(), {.tint = tint});
        screen.nine_slice(layer.texture(), {.position = {8, 0}, .borders = {1, 1, 1, 1}, .tint = tint});
        const auto image = out.read();
        CHECK(tgx_test::nearly(image[1, 1], expected[i], 1));
        CHECK(tgx_test::nearly(image[9, 1], expected[i], 1));
        CHECK(image[1, 6] == tgx::colors::transparent);
        CHECK(image[9, 6] == tgx::colors::transparent);
    }
}
