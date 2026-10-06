// A shader of its own for the Canvas, and shaders changing or going while
// shapes drawn with them wait in the batch.

#include "gpu.h"

#include <doctest/doctest.h>

#include <utility>

namespace {
    // What the built-in canvas shader takes.
    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec2 in_uv;
        layout(location = 2) in vec4 in_color;
        uniform mat4 u_projection;
        out vec4 color;
        void main() {
            gl_Position = u_projection * vec4(in_position, 0.0, 1.0);
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
        auto shader = tgx::gl::Shader::from_source(vertex_source, fragment_source);
        REQUIRE(shader.has_value());
        return std::move(*shader);
    }

    [[nodiscard]] auto tint(tgx::Color color) -> tgx::gl::Shader {
        tgx::gl::Shader tinting = shader(tint_source);
        tinting.set(tinting.uniform<tgx::Color>("u_tint"), color);
        return tinting;
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
