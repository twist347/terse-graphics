// Failed setup must leave older resources usable, including their storage
// and cached bindings. Inject errors without exhausting driver memory.

#include "gpu.hpp"

#include "gpu/context.hpp"

#include <doctest/doctest.h>
#include <glad/gl.h>

#include <array>
#include <utility>

namespace {
    enum class Object { texture, buffer, renderbuffer };
    enum class Step { generation, binding, storage, zero_id };

    // Generation, binding and deletion have identical signatures for these
    // objects. Storage differs, so each storage callback forwards its own GL
    // call. Only the selected kind is intercepted.
    class StorageFailure {
    public:
        StorageFailure(Object object, Step step, GLenum error) noexcept
            : object_{object}, step_{step}, error_{error},
              generate_slot_{generation_slot(object)}, bind_slot_{binding_slot(object)}, delete_slot_{deletion_slot(object)},
              generate_{std::exchange(generate_slot_, generate)}, bind_{std::exchange(bind_slot_, bind)},
              delete_{std::exchange(delete_slot_, delete_objects)},
              texture_storage_{std::exchange(glad_glTexImage2D, texture_storage)},
              buffer_storage_{std::exchange(glad_glBufferData, buffer_storage)},
              depth_storage_{std::exchange(glad_glRenderbufferStorage, depth_storage)},
              get_error_{std::exchange(glad_glGetError, get_error)} {
            current_ = this;
        }
        StorageFailure(const StorageFailure &) = delete;
        auto operator=(const StorageFailure &) -> StorageFailure & = delete;
        ~StorageFailure() {
            generate_slot_ = generate_;
            bind_slot_ = bind_;
            delete_slot_ = delete_;
            glad_glTexImage2D = texture_storage_;
            glad_glBufferData = buffer_storage_;
            glad_glRenderbufferStorage = depth_storage_;
            glad_glGetError = get_error_;
            current_ = nullptr;
        }

        GLuint generated{0};
        bool deleted{false};
        int binds{0};
        int writes{0};

    private:
        static auto generation_slot(Object object) -> PFNGLGENBUFFERSPROC & {
            switch (object) {
                case Object::texture: return glad_glGenTextures;
                case Object::buffer: return glad_glGenBuffers;
                case Object::renderbuffer: return glad_glGenRenderbuffers;
            }
            return glad_glGenBuffers;
        }
        static auto binding_slot(Object object) -> PFNGLBINDBUFFERPROC & {
            switch (object) {
                case Object::texture: return glad_glBindTexture;
                case Object::buffer: return glad_glBindBuffer;
                case Object::renderbuffer: return glad_glBindRenderbuffer;
            }
            return glad_glBindBuffer;
        }
        static auto deletion_slot(Object object) -> PFNGLDELETEBUFFERSPROC & {
            switch (object) {
                case Object::texture: return glad_glDeleteTextures;
                case Object::buffer: return glad_glDeleteBuffers;
                case Object::renderbuffer: return glad_glDeleteRenderbuffers;
            }
            return glad_glDeleteBuffers;
        }
        static void GLAD_API_PTR generate(GLsizei count, GLuint *ids) {
            if (current_->step_ == Step::zero_id) {
                for (GLsizei i = 0; i < count; ++i) {
                    ids[i] = 0;
                }
                return;
            }
            current_->generate_(count, ids);
            current_->generated = ids[0];
            // A partial allocation must also be deleted on error.
            current_->pending_ = current_->step_ == Step::generation;
        }
        static void GLAD_API_PTR bind(GLenum target, GLuint id) {
            ++current_->binds;
            if (current_->step_ == Step::binding) {
                current_->pending_ = true;
                return;
            }
            current_->bind_(target, id);
        }
        static void GLAD_API_PTR delete_objects(GLsizei count, const GLuint *ids) {
            for (GLsizei i = 0; i < count; ++i) {
                current_->deleted |= ids[i] != 0 && ids[i] == current_->generated;
            }
            current_->delete_(count, ids);
        }
        static auto reject_storage(Object object) -> bool {
            if (current_->object_ != object) {
                return false;
            }
            ++current_->writes;
            if (current_->step_ == Step::storage) {
                current_->pending_ = true;
                return true;
            }
            return false;
        }
        static void GLAD_API_PTR texture_storage(
            GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
            GLint border, GLenum format, GLenum type, const void *pixels
        ) {
            if (!reject_storage(Object::texture)) {
                current_->texture_storage_(target, level, internal, width, height, border, format, type, pixels);
            }
        }
        static void GLAD_API_PTR buffer_storage(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
            if (!reject_storage(Object::buffer)) {
                current_->buffer_storage_(target, size, data, usage);
            }
        }
        static void GLAD_API_PTR depth_storage(GLenum target, GLenum format, GLsizei width, GLsizei height) {
            if (!reject_storage(Object::renderbuffer)) {
                current_->depth_storage_(target, format, width, height);
            }
        }
        static auto GLAD_API_PTR get_error() -> GLenum {
            return std::exchange(current_->pending_, false) ? current_->error_ : current_->get_error_();
        }

        static inline StorageFailure *current_{nullptr};
        Object object_;
        Step step_;
        GLenum error_;
        bool pending_{false};
        PFNGLGENBUFFERSPROC &generate_slot_;
        PFNGLBINDBUFFERPROC &bind_slot_;
        PFNGLDELETEBUFFERSPROC &delete_slot_;
        PFNGLGENBUFFERSPROC generate_;
        PFNGLBINDBUFFERPROC bind_;
        PFNGLDELETEBUFFERSPROC delete_;
        PFNGLTEXIMAGE2DPROC texture_storage_;
        PFNGLBUFFERDATAPROC buffer_storage_;
        PFNGLRENDERBUFFERSTORAGEPROC depth_storage_;
        PFNGLGETERRORPROC get_error_;
    };

    template<typename Test>
    auto failures(Test test) -> void {
        for (const auto step: {Step::generation, Step::binding, Step::storage, Step::zero_id}) {
            for (const GLenum error: std::array<GLenum, 3>{GL_OUT_OF_MEMORY, GL_INVALID_OPERATION, GL_NO_ERROR}) {
                if ((step == Step::zero_id) != (error == GL_NO_ERROR)) {
                    continue;
                }
                CAPTURE(static_cast<int>(step));
                CAPTURE(error);
                test(step, error);
            }
        }
    }

    template<typename T>
    auto check_failure(const tgx::Result<T> &result, const StorageFailure &failure, Step step, GLenum error) -> void {
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == (error == GL_OUT_OF_MEMORY ? tgx::Error::out_of_memory : tgx::Error::platform));
        CHECK(failure.deleted == (failure.generated != 0));
        CHECK(failure.binds == (step == Step::binding || step == Step::storage ? 1 : 0));
        CHECK(failure.writes == (step == Step::storage ? 1 : 0));
        CHECK(glGetError() == GL_NO_ERROR);
    }
}

TEST_CASE("texture setup failures preserve previous pixels and cached bindings") {
    failures([](Step step, GLenum error) {
        const auto target = tgx_test::target({8, 8});
        const auto previous = tgx_test::plain(tgx::colors::red);
        const auto incoming = tgx::Image::create({4, 4}, tgx::colors::green);
        {
            StorageFailure failure{Object::texture, step, error};
            const auto result = tgx::Texture::create(incoming, {.mipmaps = true});
            check_failure(result, failure, step, error);
        }
        if (step != Step::storage) {
            GLint bound = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
            CHECK(static_cast<tgx::GlId>(bound) == previous.id());
            // A failed bind must not make cleanup cache 0 while the old
            // texture remains bound. Unbinding must reach GL too.
            REQUIRE(tgx::detail::context().try_bind_texture(0).has_value());
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
            CHECK(bound == 0);
        }
        REQUIRE(tgx::detail::context().try_bind_texture(previous.id()).has_value());
        GLint width = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        REQUIRE(width == 1);
        auto canvas = tgx_test::canvas_on(target);
        canvas.sprite(previous, {.size = {8, 8}});
        CHECK(target.read()[4, 4] == tgx::colors::red);

        const auto after = tgx_test::texture(incoming);
        canvas.sprite(after, {.size = {8, 8}});
        CHECK(target.read()[4, 4] == tgx::colors::green);
        CHECK(glGetError() == GL_NO_ERROR);
    });
}

TEST_CASE("buffer setup failures preserve previous storage and draws") {
    failures([](Step step, GLenum error) {
        const auto target = tgx_test::target({8, 8});
        const auto previous = tgx_test::Cover::create();
        constexpr std::array incoming{9.f, 9.f};
        {
            StorageFailure failure{Object::buffer, step, error};
            const auto result = tgx::gl::Buffer<float>::create(incoming);
            check_failure(result, failure, step, error);
        }
        if (step != Step::storage) {
            GLint bound = 0;
            // GL 3.3 uses the target enum itself for this binding query.
            glGetIntegerv(GL_COPY_WRITE_BUFFER, &bound);
            CHECK(static_cast<tgx::GlId>(bound) == previous.buffer.id());
        }
        glBindBuffer(GL_COPY_WRITE_BUFFER, previous.buffer.id());
        GLint bytes = 0;
        glGetBufferParameteriv(GL_COPY_WRITE_BUFFER, GL_BUFFER_SIZE, &bytes);
        std::array<tgx_test::Cover::Vertex, 3> vertices{};
        REQUIRE(bytes == sizeof(vertices));
        glGetBufferSubData(GL_COPY_WRITE_BUFFER, 0, sizeof(vertices), vertices.data());
        CHECK(vertices[0].position == tgx::Vec2{-1, -1});
        CHECK(vertices[1].position == tgx::Vec2{3, -1});
        CHECK(vertices[2].position == tgx::Vec2{-1, 3});
        previous.draw({.target = &target});
        CHECK(target.read()[4, 4] == tgx::colors::green);

        const auto after = tgx_test::Cover::create();
        after.draw({.target = &target});
        CHECK(target.read()[4, 4] == tgx::colors::green);
        CHECK(glGetError() == GL_NO_ERROR);
    });
}

TEST_CASE("depth storage setup failures preserve previous attachments and depth draws") {
    failures([](Step step, GLenum error) {
        const auto cover = tgx_test::Cover::create();
        const auto previous = tgx_test::target({8, 8}, true);
        auto &device = tgx_test::app().device();
        device.clear({.target = &previous, .color = tgx::colors::red, .depth = 1.f});
        GLint depth = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depth);
        {
            StorageFailure failure{Object::renderbuffer, step, error};
            const auto result = tgx::RenderTarget::create({4, 4}, {.depth = true});
            check_failure(result, failure, step, error);
        }
        GLint binding = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &binding);
        CHECK(static_cast<tgx::GlId>(binding) == previous.id());
        GLint attached = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &attached);
        REQUIRE(attached == depth);
        if (step != Step::storage) {
            glGetIntegerv(GL_RENDERBUFFER_BINDING, &binding);
            CHECK(binding == depth);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(depth));
        GLint width = 0, height = 0;
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
        REQUIRE(width == 8);
        REQUIRE(height == 8);
        CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        CHECK(previous.read()[4, 4] == tgx::colors::red);
        cover.draw({.target = &previous, .state = {.depth = tgx::gl::Depth::less}});
        CHECK(previous.read()[4, 4] == tgx::colors::green);

        const auto after = tgx_test::target({4, 4}, true);
        cover.draw({.target = &after, .state = {.depth = tgx::gl::Depth::less}});
        CHECK(after.read()[2, 2] == tgx::colors::green);
        CHECK(glGetError() == GL_NO_ERROR);
    });
}
