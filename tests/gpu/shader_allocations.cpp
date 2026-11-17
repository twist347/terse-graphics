#include "gpu.hpp"

#include "gpu/context.hpp"

#include <doctest/doctest.h>
#include <glad/gl.h>

#include <array>
#include <string>
#include <utility>

namespace {
    enum class Step { vertex, fragment, program, source, compilation, linking };

    class ShaderFailure {
    public:
        ShaderFailure(Step step, GLenum error, bool partial = false, GLenum second_error = GL_NO_ERROR) noexcept
            : step_{step}, error_{error}, partial_{partial}, second_error_{second_error},
              create_shader_{std::exchange(glad_glCreateShader, create_shader)},
              create_program_{std::exchange(glad_glCreateProgram, create_program)},
              delete_shader_{std::exchange(glad_glDeleteShader, delete_shader)},
              delete_program_{std::exchange(glad_glDeleteProgram, delete_program)},
              source_{std::exchange(glad_glShaderSource, source)},
              compile_{std::exchange(glad_glCompileShader, compile)},
              link_{std::exchange(glad_glLinkProgram, link)},
              get_error_{std::exchange(glad_glGetError, get_error)} {
            current_ = this;
        }
        ShaderFailure(const ShaderFailure &) = delete;
        auto operator=(const ShaderFailure &) -> ShaderFailure & = delete;
        ~ShaderFailure() {
            glad_glCreateShader = create_shader_;
            glad_glCreateProgram = create_program_;
            glad_glDeleteShader = delete_shader_;
            glad_glDeleteProgram = delete_program_;
            glad_glShaderSource = source_;
            glad_glCompileShader = compile_;
            glad_glLinkProgram = link_;
            glad_glGetError = get_error_;
            current_ = nullptr;
        }

        std::array<GLuint, 2> stages{};
        int stage_count{0};
        int deleted_stages{0};
        GLuint program{0};
        bool deleted_program{false};

    private:
        auto raise() noexcept -> void { pending_ = {error_, second_error_}; }

        static auto GLAD_API_PTR create_shader(GLenum type) -> GLuint {
            const bool fail = (current_->step_ == Step::vertex && type == GL_VERTEX_SHADER)
                              || (current_->step_ == Step::fragment && type == GL_FRAGMENT_SHADER);
            if (fail) {
                current_->raise();
                if (!current_->partial_) {
                    return 0;
                }
            }
            const GLuint id = current_->create_shader_(type);
            current_->stages[current_->stage_count++] = id;
            return id;
        }
        static auto GLAD_API_PTR create_program() -> GLuint {
            if (current_->step_ == Step::program) {
                current_->raise();
                if (!current_->partial_) {
                    return 0;
                }
            }
            current_->program = current_->create_program_();
            return current_->program;
        }
        static void GLAD_API_PTR delete_shader(GLuint id) {
            ++current_->deleted_stages;
            current_->delete_shader_(id);
        }
        static void GLAD_API_PTR delete_program(GLuint id) {
            current_->deleted_program = id != 0 && id == current_->program;
            current_->delete_program_(id);
        }
        static void GLAD_API_PTR source(GLuint shader, GLsizei count, const GLchar *const *strings, const GLint *lengths) {
            if (current_->step_ == Step::source) {
                current_->raise();
                return;
            }
            current_->source_(shader, count, strings, lengths);
        }
        static void GLAD_API_PTR compile(GLuint shader) {
            if (current_->step_ == Step::compilation) {
                current_->raise();
                return;
            }
            current_->compile_(shader);
        }
        static void GLAD_API_PTR link(GLuint program) {
            if (current_->step_ == Step::linking) {
                current_->raise();
                return;
            }
            current_->link_(program);
        }
        static auto GLAD_API_PTR get_error() -> GLenum {
            if (current_->pending_[0] != GL_NO_ERROR) {
                return std::exchange(current_->pending_[0], std::exchange(current_->pending_[1], GL_NO_ERROR));
            }
            return current_->get_error_();
        }

        static inline ShaderFailure *current_{nullptr};
        Step step_;
        GLenum error_;
        bool partial_;
        GLenum second_error_;
        std::array<GLenum, 2> pending_{};
        PFNGLCREATESHADERPROC create_shader_;
        PFNGLCREATEPROGRAMPROC create_program_;
        PFNGLDELETESHADERPROC delete_shader_;
        PFNGLDELETEPROGRAMPROC delete_program_;
        PFNGLSHADERSOURCEPROC source_;
        PFNGLCOMPILESHADERPROC compile_;
        PFNGLLINKPROGRAMPROC link_;
        PFNGLGETERRORPROC get_error_;
    };

    constexpr const char *vertex = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 position;
        void main() { gl_Position = vec4(position, 0.0, 1.0); }
    )";
    constexpr const char *fragment = TGX_GLSL_VERSION R"(
        out vec4 color;
        void main() { color = vec4(0.0, 1.0, 0.0, 1.0); }
    )";
}

TEST_CASE("shader setup distinguishes driver errors and releases partial objects") {
    const auto previous = tgx_test::Cover::create();
    const auto target = tgx_test::target({8, 8});
    for (const auto step: {Step::vertex, Step::fragment, Step::program, Step::source, Step::compilation, Step::linking}) {
        const bool creation = step == Step::vertex || step == Step::fragment || step == Step::program;
        for (const GLenum error: std::array<GLenum, 3>{GL_OUT_OF_MEMORY, GL_INVALID_OPERATION, GL_NO_ERROR}) {
            for (const bool partial: {false, true}) {
                if ((!creation && (partial || error == GL_NO_ERROR)) || (partial && error == GL_NO_ERROR)) {
                    continue;
                }
                CAPTURE(static_cast<int>(step));
                CAPTURE(error);
                CAPTURE(partial);
                previous.draw({.target = &target});
                {
                    ShaderFailure failure{step, error, partial};
                    std::string log{"stale marker"};
                    const auto result = tgx::gl::Shader::from_source(vertex, fragment, &log);
                    REQUIRE_FALSE(result.has_value());
                    CHECK(result.error() == (error == GL_OUT_OF_MEMORY ? tgx::Error::out_of_memory : tgx::Error::platform));
                    CHECK_FALSE(log.contains("stale marker"));
                    CHECK(failure.deleted_stages == failure.stage_count);
                    for (int i = 0; i < failure.stage_count; ++i) {
                        CHECK(glIsShader(failure.stages[i]) == GL_FALSE);
                    }
                    CHECK(failure.deleted_program == (failure.program != 0));
                    if (failure.program != 0) {
                        CHECK(glIsProgram(failure.program) == GL_FALSE);
                    }
                    CHECK(glGetError() == GL_NO_ERROR);
                }
                tgx_test::app().device().clear({.target = &target, .color = tgx::colors::black});
                previous.draw({.target = &target});
                CHECK(target.read()[4, 4] == tgx::colors::green);
                const auto after = tgx_test::required(tgx::gl::Shader::from_source(vertex, fragment));
                tgx_test::app().device().draw(after, previous.vertices, {.target = &target});
                CHECK(target.read()[4, 4] == tgx::colors::green);
                CHECK(glGetError() == GL_NO_ERROR);
            }
        }
    }
}

TEST_CASE("shader setup consumes multiple GL error flags and preserves out_of_memory") {
    ShaderFailure failure{Step::compilation, GL_INVALID_OPERATION, false, GL_OUT_OF_MEMORY};
    const auto result = tgx::gl::Shader::from_source(vertex, fragment);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == tgx::Error::out_of_memory);
    CHECK(failure.deleted_stages == failure.stage_count);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Canvas GPU setup returns shader allocation failures and can be retried") {
    auto &context = tgx::detail::context();
    for (const auto step: {Step::vertex, Step::fragment, Step::program}) {
        for (const GLenum error: std::array<GLenum, 2>{GL_OUT_OF_MEMORY, GL_INVALID_OPERATION}) {
            CAPTURE(static_cast<int>(step));
            CAPTURE(error);
            context.close();
            {
                ShaderFailure failure{step, error};
                const auto opened = context.open();
                CHECK_FALSE(opened.has_value());
                if (!opened) {
                    CHECK(opened.error() == (error == GL_OUT_OF_MEMORY ? tgx::Error::out_of_memory : tgx::Error::platform));
                }
                CHECK(failure.deleted_stages == failure.stage_count);
                CHECK(glGetError() == GL_NO_ERROR);
            }
            // Restore the shared App even after a failed check above.
            REQUIRE(context.open().has_value());
            const auto cover = tgx_test::Cover::create();
            const auto target = tgx_test::target({8, 8});
            cover.draw({.target = &target});
            CHECK(target.read()[4, 4] == tgx::colors::green);
        }
    }
}
