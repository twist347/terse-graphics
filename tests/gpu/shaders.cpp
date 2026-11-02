// A shader of its own for the Canvas, and shaders changing or going while
// shapes drawn with them wait in the batch.

#include "gpu.hpp"

#include <doctest/doctest.h>
#include <glad/gl.h>

#include <array>
#include <string>
#include <utility>

namespace {
    // Invalid GLSL is expected to produce compiler diagnostics. Keep other
    // GL sources enabled, and restore just these severities afterwards.
    struct ShaderCompilerMessages {
        ShaderCompilerMessages() { enable(false); }
        ShaderCompilerMessages(const ShaderCompilerMessages &) = delete;
        auto operator=(const ShaderCompilerMessages &) -> ShaderCompilerMessages & = delete;
        ~ShaderCompilerMessages() { enable(true); }

    private:
        static auto enable(bool enabled) noexcept -> void {
            if (GLAD_GL_KHR_debug != 0) {
                for (const GLenum severity: {GL_DEBUG_SEVERITY_HIGH, GL_DEBUG_SEVERITY_MEDIUM}) {
                    glDebugMessageControl(GL_DEBUG_SOURCE_SHADER_COMPILER, GL_DONT_CARE, severity, 0, nullptr, enabled ? GL_TRUE : GL_FALSE);
                }
            }
        }
    };

    [[nodiscard]] auto invalid_shader(const char *vertex, const char *fragment, std::string &log) -> tgx::Result<tgx::gl::Shader> {
        const ShaderCompilerMessages expected{};
        return tgx::gl::Shader::from_source(vertex, fragment, &log);
    }

    // What the built-in canvas shader takes.
    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec2 in_uv;
        layout(location = 2) in vec4 in_color;
        uniform mat4 u_projection;
        out vec2 uv;
        out vec4 color;
        void main() {
            gl_Position = u_projection * vec4(in_position, 0.0, 1.0);
            uv = in_uv;
            color = in_color;
        }
    )";

    constexpr const char *inverting_source = TGX_GLSL_VERSION R"(
        in vec4 color;
        out vec4 out_color;
        void main() { out_color = vec4(1.0 - color.rgb, 1.0); }
    )";

    // The color of u_tint, whatever the shape's.
    constexpr const char *tint_source = TGX_GLSL_VERSION R"(
        in vec4 color;
        uniform vec4 u_tint;
        out vec4 out_color;
        void main() { out_color = u_tint + color * 0.0; }
    )";

    [[nodiscard]] auto shader(const char *fragment_source) -> tgx::gl::Shader {
        return tgx_test::required(tgx::gl::Shader::from_source(vertex_source, fragment_source));
    }

    [[nodiscard]] auto tint(tgx::Color color) -> tgx::gl::Shader {
        tgx::gl::Shader tinting = shader(tint_source);
        tinting.set(tinting.uniform<tgx::Color>("u_tint"), color);
        return tinting;
    }
}

TEST_CASE("shader compilation reports the failing stages and replaces out_log") {
    constexpr const char *bad_vertex = TGX_GLSL_VERSION R"(
        void main() { gl_Position = ; }
    )";
    constexpr const char *bad_fragment = TGX_GLSL_VERSION R"(
        out vec4 out_color;
        void main() { out_color = ; }
    )";
    std::string log{"stale marker"};
    {
        const auto failed = invalid_shader(vertex_source, bad_fragment, log);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error() == tgx::Error::compile);
        CHECK(log.contains("fragment:\n"));
        CHECK_FALSE(log.contains("vertex:\n"));
        CHECK_FALSE(log.contains("stale marker"));
    }
    {
        const auto failed = invalid_shader(bad_vertex, inverting_source, log);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error() == tgx::Error::compile);
        CHECK(log.contains("vertex:\n"));
        CHECK_FALSE(log.contains("fragment:\n"));
    }
    {
        const auto failed = invalid_shader(bad_vertex, bad_fragment, log);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error() == tgx::Error::compile);
        CHECK(log.contains("vertex:\n"));
        CHECK(log.contains("fragment:\n"));
    }
    log = "stale marker";
    const auto valid = tgx::gl::Shader::from_source(vertex_source, inverting_source, &log);
    CHECK(valid.has_value());
    CHECK_FALSE(log.contains("stale marker"));
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("shader linking reports conflicting uniform types") {
    constexpr const char *vertex = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        uniform float u_conflict;
        void main() {
            gl_Position = vec4(in_position * u_conflict, 0.0, 1.0);
        }
    )";
    constexpr const char *fragment = TGX_GLSL_VERSION R"(
        uniform vec4 u_conflict;
        out vec4 out_color;
        void main() { out_color = u_conflict; }
    )";
    std::string log{"stale marker"};
    const auto failed = invalid_shader(vertex, fragment, log);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error() == tgx::Error::link);
    CHECK(log.contains("link:\n"));
    CHECK_FALSE(log.contains("stale marker"));
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("linked sampler uniforms remain visible when tgx cannot supply their texture type") {
    struct Case {
        const char *fragment;
        GLenum type;
    };
    const std::array cases{
        Case{TGX_GLSL_VERSION R"(
            uniform samplerCube u_texture;
            out vec4 out_color;
            void main() { out_color = texture(u_texture, vec3(0.0, 0.0, 1.0)); }
        )", GL_SAMPLER_CUBE},
        Case{TGX_GLSL_VERSION R"(
            uniform sampler2DShadow u_texture;
            out vec4 out_color;
            void main() { out_color = vec4(texture(u_texture, vec3(0.5))); }
        )", GL_SAMPLER_2D_SHADOW},
        Case{TGX_GLSL_VERSION R"(
            uniform isampler2D u_texture;
            out vec4 out_color;
            void main() { out_color = vec4(texelFetch(u_texture, ivec2(0), 0)); }
        )", GL_INT_SAMPLER_2D},
        Case{TGX_GLSL_VERSION R"(
            uniform usampler2DMS u_texture;
            out vec4 out_color;
            void main() { out_color = vec4(texelFetch(u_texture, ivec2(0), 0)); }
        )", GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE},
    };
    for (const auto &test: cases) {
        CAPTURE(test.fragment);
        const auto linked = shader(test.fragment);
        const auto samplers = tgx::gl::detail::samplers(linked);
        REQUIRE(samplers.size() == 1);
        CHECK(samplers[0].name == "u_texture");
        CHECK(samplers[0].gl_type == test.type);
        CHECK(samplers[0].location == glGetUniformLocation(linked.id(), "u_texture"));
        CHECK(glGetError() == GL_NO_ERROR);
    }
}

TEST_CASE("a canvas shader for the shapes after it, the built-in one back after that") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const tgx::gl::Shader inverting = shader(inverting_source);

    canvas.clear(tgx::colors::black);
    canvas.set_shader(&inverting);
    canvas.rect({0, 0, 20, 20}, tgx::colors::red);
    canvas.set_shader(nullptr);
    canvas.rect({20, 0, 20, 20}, tgx::colors::red);

    const tgx::Image image = target.read();
    CHECK(image[10, 10] == tgx::colors::cyan);
    CHECK(image[30, 10] == tgx::colors::red);
}

TEST_CASE("canvas shaders accept the batch layout and optimized-out inputs, with an optional sampler") {
    const auto target = tgx_test::target({12, 4});
    auto canvas = tgx_test::canvas_on(target);
    auto textured = shader(TGX_GLSL_VERSION R"(
        in vec2 uv;
        in vec4 color;
        uniform sampler2D u_texture;
        out vec4 out_color;
        void main() { out_color = texture(u_texture, uv) * color; }
    )");
    const auto position_only = shader(TGX_GLSL_VERSION R"(
        out vec4 out_color;
        void main() { out_color = vec4(0.0, 1.0, 0.0, 1.0); }
    )");

    // Canvas must set its sampler to slot 0, regardless of its old value.
    textured.set(textured.uniform<tgx::gl::TextureSlot>("u_texture"), {3});
    canvas.clear(tgx::colors::black);
    canvas.set_shader(&textured);
    canvas.rect({0, 0, 4, 4}, tgx::colors::red);
    // The linked program reads only position; UV, color and sampler are absent.
    canvas.set_shader(&position_only);
    canvas.rect({4, 0, 4, 4}, tgx::colors::red);
    canvas.set_shader(nullptr);
    canvas.rect({8, 0, 4, 4}, tgx::colors::yellow);

    const auto image = target.read();
    CHECK(image[2, 2] == tgx::colors::red);
    CHECK(image[6, 2] == tgx::colors::green);
    CHECK(image[10, 2] == tgx::colors::yellow);
    GLint slot = -1;
    glGetUniformiv(textured.id(), glGetUniformLocation(textured.id(), "u_texture"), &slot);
    CHECK(slot == 0);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("a uniform set while shapes drawn with the shader wait") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    tgx::gl::Shader tinting = shader(tint_source);
    const auto u_tint = tinting.uniform<tgx::Color>("u_tint");

    canvas.clear(tgx::colors::black);
    tinting.set(u_tint, tgx::colors::red);
    canvas.set_shader(&tinting);
    canvas.rect({0, 0, 20, 20}, tgx::colors::white);
    tinting.set(u_tint, tgx::colors::green);
    canvas.rect({20, 0, 20, 20}, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image[10, 10] == tgx::colors::red);
    CHECK(image[30, 10] == tgx::colors::green);
}

TEST_CASE("a shader destroyed while shapes drawn with it wait") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    {
        const tgx::gl::Shader doomed = tint(tgx::colors::magenta);
        canvas.set_shader(&doomed);
        canvas.rect({0, 0, 20, 20}, tgx::colors::white);
        canvas.set_shader(nullptr);
    }
    canvas.rect({20, 0, 20, 20}, tgx::colors::yellow);

    const tgx::Image image = target.read();
    CHECK(image[10, 10] == tgx::colors::magenta);
    CHECK(image[30, 10] == tgx::colors::yellow);
}

TEST_CASE("a shader moved while shapes drawn with it wait") {
    const tgx::RenderTarget target = tgx_test::target({64, 64});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.clear(tgx::colors::black);
    tgx::gl::Shader a = tint(tgx::colors::cyan);
    canvas.set_shader(&a);
    canvas.rect({0, 0, 20, 20}, tgx::colors::white);
    const tgx::gl::Shader b = std::move(a);
    canvas.set_shader(&b);
    canvas.rect({20, 0, 20, 20}, tgx::colors::white);

    const tgx::Image image = target.read();
    CHECK(image[10, 10] == tgx::colors::cyan);
    CHECK(image[30, 10] == tgx::colors::cyan);
}
