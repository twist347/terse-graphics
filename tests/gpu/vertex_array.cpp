// Driver failures while creating and configuring a vertex array.

#include "gpu.hpp"

#include <doctest/doctest.h>
#include <glad/gl.h>

#include <array>
#include <utility>

namespace {
    enum class FailureAt {
        generation,
        binding,
        attributes,
    };

    // Injects an error without exhausting driver memory. Restores GL calls
    // even when a REQUIRE ends the test early.
    class VertexArrayFailure {
    public:
        VertexArrayFailure(FailureAt at, GLenum error) noexcept
            : at_{at}, error_{error},
              generate_{std::exchange(glad_glGenVertexArrays, generate)},
              bind_{std::exchange(glad_glBindVertexArray, bind)},
              enable_{std::exchange(glad_glEnableVertexAttribArray, enable)},
              get_error_{std::exchange(glad_glGetError, get_error)} {
            current_ = this;
        }

        VertexArrayFailure(const VertexArrayFailure &) = delete;
        auto operator=(const VertexArrayFailure &) -> VertexArrayFailure & = delete;
        VertexArrayFailure(VertexArrayFailure &&) = delete;
        auto operator=(VertexArrayFailure &&) -> VertexArrayFailure & = delete;

        ~VertexArrayFailure() {
            glad_glGenVertexArrays = generate_;
            glad_glBindVertexArray = bind_;
            glad_glEnableVertexAttribArray = enable_;
            glad_glGetError = get_error_;
            current_ = nullptr;
        }

        GLuint generated{0};
        int binds{0};

    private:
        static void GLAD_API_PTR generate(GLsizei count, GLuint *ids) {
            if (current_->at_ == FailureAt::generation) {
                for (GLsizei i = 0; i < count; ++i) {
                    ids[i] = 0;
                }
                current_->pending_ = true;
                return;
            }
            current_->generate_(count, ids);
            current_->generated = ids[0];
        }

        static void GLAD_API_PTR bind(GLuint id) {
            ++current_->binds;
            if (current_->at_ == FailureAt::binding) {
                current_->pending_ = true;
            } else {
                current_->bind_(id);
            }
        }

        static void GLAD_API_PTR enable(GLuint index) {
            current_->enable_(index);
            if (current_->at_ == FailureAt::attributes) {
                current_->pending_ = true;
            }
        }

        static auto GLAD_API_PTR get_error() -> GLenum {
            return std::exchange(current_->pending_, false) ? current_->error_ : current_->get_error_();
        }

        static inline VertexArrayFailure *current_{nullptr};
        FailureAt at_;
        GLenum error_;
        bool pending_{false};
        PFNGLGENVERTEXARRAYSPROC generate_;
        PFNGLBINDVERTEXARRAYPROC bind_;
        PFNGLENABLEVERTEXATTRIBARRAYPROC enable_;
        PFNGLGETERRORPROC get_error_;
    };
}

TEST_CASE("vertex array creation reports driver errors and releases failed objects") {
    using Vertex = tgx_test::Cover::Vertex;
    const std::array layout{tgx::gl::VertexAttribute::of(0, &Vertex::position)};
    const std::array other_layout{tgx::gl::VertexAttribute::of(1, &Vertex::position)};

    for (const auto at: {FailureAt::generation, FailureAt::binding, FailureAt::attributes}) {
        for (const GLenum error: std::array<GLenum, 2>{GL_OUT_OF_MEMORY, GL_INVALID_OPERATION}) {
            CAPTURE(static_cast<int>(at));
            CAPTURE(error);
            const auto previous = tgx::gl::VertexArray::create<Vertex>(layout);
            REQUIRE(previous.has_value());
            {
                VertexArrayFailure failure{at, error};
                const auto result = tgx::gl::VertexArray::create<Vertex>(other_layout);
                REQUIRE_FALSE(result.has_value());
                CHECK(result.error() == (error == GL_OUT_OF_MEMORY ? tgx::Error::out_of_memory : tgx::Error::platform));
                if (at == FailureAt::generation) {
                    CHECK(failure.binds == 0);
                } else {
                    REQUIRE(failure.generated != 0);
                    CHECK(glIsVertexArray(failure.generated) == GL_FALSE);
                }
            }
            if (at == FailureAt::binding) {
                // A rejected bind leaves the previous VAO current. Its layout
                // must not be changed by the failed creation.
                GLint enabled = GL_TRUE;
                glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
                CHECK(enabled == GL_FALSE);
            }
            CHECK(glGetError() == GL_NO_ERROR);

            // Both overloads remain usable after a failed allocation.
            const auto untyped = tgx::gl::VertexArray::create(sizeof(Vertex), layout);
            REQUIRE(untyped.has_value());
            CHECK(glIsVertexArray(untyped->id()) == GL_TRUE);
            const auto cover = tgx_test::Cover::create();
            const auto target = tgx_test::target({8, 8});
            cover.draw({.target = &target});
            CHECK(target.read()[4, 4] == tgx::colors::green);
        }
    }
}
