// Your own draws (Device::draw): textures in slots, render state given per
// draw, vertex formats and uniform types. The Device remembers what GL has
// set and changes only what differs, so most of these check that nothing
// carries over from one draw, or from the Canvas, into the next.

#include "gpu.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <utility>

namespace {
    // Red from u_a, green from u_b.
    constexpr const char *two_samplers_source = TGX_GLSL_VERSION R"(
        uniform sampler2D u_a;
        uniform sampler2D u_b;
        out vec4 out_color;
        void main() {
            out_color = vec4(texture(u_a, vec2(0.5)).r, texture(u_b, vec2(0.5)).g, 0.0, 1.0);
        }
    )";

    // The color of u_color.
    constexpr const char *uniform_color_source = TGX_GLSL_VERSION R"(
        uniform vec4 u_color;
        out vec4 out_color;
        void main() { out_color = u_color; }
    )";

    [[nodiscard]] auto colored(tgx::Color color) -> tgx::gl::Shader {
        tgx::gl::Shader shader = tgx_test::cover_shader(uniform_color_source);
        shader.set(shader.uniform<tgx::Color>("u_color"), color);
        return shader;
    }
}

TEST_CASE("textures in slots, each sampler reading its own") {
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    const auto cover = tgx_test::Cover::create();
    tgx::gl::Shader shader = tgx_test::cover_shader(two_samplers_source);
    shader.set(shader.uniform<tgx::gl::TextureSlot>("u_a"), {0});
    shader.set(shader.uniform<tgx::gl::TextureSlot>("u_b"), {3});
    const tgx::Texture red = tgx_test::plain(tgx::colors::red);
    const tgx::Texture white = tgx_test::plain(tgx::colors::white);
    auto &device = tgx_test::app().device();

    device.draw(shader, cover.vertices, {.target = &target, .textures = {&red, nullptr, nullptr, &white}});
    CHECK(target.read()[4, 4] == tgx::colors::yellow);

    // Swapped: red has no green, white has red.
    device.draw(shader, cover.vertices, {.target = &target, .textures = {&white, nullptr, nullptr, &red}});
    CHECK(target.read()[4, 4] == tgx::colors::red);
}

TEST_CASE("a slot set in the shader with layout(binding)") {
    // An extension; macOS lacks it.
    auto shader = tgx::gl::Shader::from_source(
        TGX_GLSL_VERSION R"(
            layout(location = 0) in vec2 in_position;
            void main() { gl_Position = vec4(in_position, 0.0, 1.0); }
        )",
        TGX_GLSL_VERSION R"(
            #extension GL_ARB_shading_language_420pack : require
            layout(binding = 2) uniform sampler2D u_a;
            out vec4 out_color;
            void main() { out_color = texture(u_a, vec2(0.5)); }
        )"
    );
    if (!shader) {
        MESSAGE("skipped: no GL_ARB_shading_language_420pack");
        return;
    }
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    const auto cover = tgx_test::Cover::create();
    const tgx::Texture red = tgx_test::plain(tgx::colors::red);

    tgx_test::app().device().draw(*shader, cover.vertices, {.target = &target, .textures = {nullptr, nullptr, &red}});
    CHECK(target.read()[4, 4] == tgx::colors::red);
}

TEST_CASE("a sprite after a draw with a texture in slot 0, and a draw after the sprite") {
    const tgx::RenderTarget target = tgx_test::target({16, 8});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const auto cover = tgx_test::Cover::create();
    const tgx::gl::Shader shader = tgx_test::cover_shader(TGX_GLSL_VERSION R"(
        uniform sampler2D u_a;
        out vec4 out_color;
        void main() { out_color = texture(u_a, vec2(0.5)); }
    )");
    const tgx::Texture red = tgx_test::plain(tgx::colors::red);
    const tgx::Texture blue = tgx_test::plain(tgx::colors::blue);
    auto &device = tgx_test::app().device();

    device.draw(shader, cover.vertices, {.target = &target, .textures = {&red}});
    canvas.sprite(blue, {.size = {8, 8}});
    CHECK(target.read()[4, 4] == tgx::colors::blue);
    CHECK(target.read()[12, 4] == tgx::colors::red);

    device.draw(shader, cover.vertices, {.target = &target, .viewport = tgx::gl::Viewport{0, 0, 8, 8}, .textures = {&red}});
    CHECK(target.read()[4, 4] == tgx::colors::red);
}

TEST_CASE("culling holds for its draw only") {
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    const auto cover = tgx_test::Cover::create();
    auto &device = tgx_test::app().device();

    device.clear({.target = &target, .color = tgx::colors::black});
    cover.draw({.target = &target, .state = {.cull = tgx::gl::Cull::front}});
    CHECK(target.read()[4, 4] == tgx::colors::black);
    cover.draw({.target = &target, .state = {.cull = tgx::gl::Cull::back}});
    CHECK(target.read()[4, 4] == tgx::colors::green);

    device.clear({.target = &target, .color = tgx::colors::black});
    cover.draw({.target = &target, .state = {.cull = tgx::gl::Cull::front}});
    cover.draw({.target = &target});
    CHECK(target.read()[4, 4] == tgx::colors::green);
}

TEST_CASE("wireframe holds for its draw only") {
    const tgx::RenderTarget target = tgx_test::target({16, 16});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    const auto cover = tgx_test::Cover::create();
    auto &device = tgx_test::app().device();

    // The triangle's edges are outside, or on the border: none in the middle.
    device.clear({.target = &target, .color = tgx::colors::black});
    cover.draw({.target = &target, .state = {.fill = tgx::gl::Fill::wireframe}});
    CHECK(target.read()[8, 8] == tgx::colors::black);

    cover.draw({.target = &target});
    CHECK(target.read()[8, 8] == tgx::colors::green);

    // The Canvas fills its shapes after a wireframe draw too.
    cover.draw({.target = &target, .state = {.fill = tgx::gl::Fill::wireframe}});
    canvas.rect({0, 0, 16, 16}, tgx::colors::blue);
    CHECK(target.read()[8, 8] == tgx::colors::blue);
}

TEST_CASE("depth: less, less_equal, no write, and none after a test") {
    const tgx::RenderTarget target = tgx_test::target({8, 8}, true);
    const auto cover = tgx_test::Cover::create();
    const tgx::gl::Shader red = colored(tgx::colors::red);
    const tgx::gl::Shader green = colored(tgx::colors::green);
    const tgx::gl::Shader blue = colored(tgx::colors::blue);
    auto &device = tgx_test::app().device();
    const auto draw = [&](const tgx::gl::Shader &shader, tgx::gl::RenderState state) {
        device.draw(shader, cover.vertices, {.target = &target, .state = state});
        return target.read()[4, 4];
    };
    using tgx::gl::Depth;

    // Every draw is at z = 0, depth 0.5.
    device.clear({.target = &target, .color = tgx::colors::black, .depth = 1.f});
    CHECK(draw(red, {.depth = Depth::less}) == tgx::colors::red);
    CHECK(draw(green, {.depth = Depth::less}) == tgx::colors::red);
    CHECK(draw(green, {.depth = Depth::less_equal}) == tgx::colors::green);

    // Without a write the depth stays at the far end, so the next one passes.
    device.clear({.target = &target, .depth = 1.f});
    CHECK(draw(blue, {.depth = Depth::less, .depth_write = false}) == tgx::colors::blue);
    CHECK(draw(red, {.depth = Depth::less}) == tgx::colors::red);

    // Depth nearer than any draw: a test fails, none draws anyway.
    device.clear({.target = &target, .depth = 0.f});
    CHECK(draw(green, {.depth = Depth::less}) == tgx::colors::red);
    CHECK(draw(green, {}) == tgx::colors::green);
}

TEST_CASE("every vertex format, read at its offset") {
    struct Vertex {
        float f{};
        tgx::Vec2 position{};
        tgx::Color color{};
        std::int32_t sint{};
        std::uint32_t uint{};
        tgx::Vec3 v3{};
        tgx::Vec4 v4{};
    };
    using tgx::gl::VertexAttribute;

    const tgx::RenderTarget target = tgx_test::target({8, 8});
    auto shader = tgx::gl::Shader::from_source(
        TGX_GLSL_VERSION R"(
            layout(location = 0) in float in_f;
            layout(location = 1) in vec2 in_position;
            layout(location = 2) in vec4 in_color;
            layout(location = 3) in int in_sint;
            layout(location = 4) in uint in_uint;
            layout(location = 5) in vec3 in_v3;
            layout(location = 6) in vec4 in_v4;
            out vec4 color;
            flat out int ok;
            void main() {
                gl_Position = vec4(in_position, 0.0, 1.0);
                color = in_color;
                ok = int(in_f == 0.25 && in_sint == -7 && in_uint == 4000000000u
                         && in_v3 == vec3(1.0, 2.0, 3.0) && in_v4 == vec4(-1.0, 0.5, 8.0, 16.0));
            }
        )",
        TGX_GLSL_VERSION R"(
            in vec4 color;
            flat in int ok;
            out vec4 out_color;
            void main() { out_color = ok == 1 ? color : vec4(1.0, 0.0, 1.0, 1.0); }
        )"
    );
    REQUIRE(shader.has_value());

    constexpr tgx::Color color{10, 200, 30, 255};
    std::array<Vertex, 3> corners{};
    const std::array<tgx::Vec2, 3> positions{{{-1, -1}, {3, -1}, {-1, 3}}};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        corners[i] = {0.25f, positions[i], color, -7, 4000000000u, {1, 2, 3}, {-1, 0.5f, 8, 16}};
    }
    auto buffer = tgx::gl::Buffer<Vertex>::create(corners);
    REQUIRE(buffer.has_value());
    auto vertices = tgx::gl::VertexArray::create<Vertex>(std::array{
        VertexAttribute::of(0, &Vertex::f),
        VertexAttribute::of(1, &Vertex::position),
        VertexAttribute::of(2, &Vertex::color),
        VertexAttribute::of(3, &Vertex::sint),
        VertexAttribute::of(4, &Vertex::uint),
        VertexAttribute::of(5, &Vertex::v3),
        VertexAttribute::of(6, &Vertex::v4),
    });
    vertices.set_vertex_buffer(*buffer);

    tgx_test::app().device().draw(*shader, vertices, {.target = &target});
    CHECK(target.read()[4, 4] == color);
}

TEST_CASE("every uniform type") {
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    const auto cover = tgx_test::Cover::create();
    tgx::gl::Shader shader = tgx_test::cover_shader(TGX_GLSL_VERSION R"(
        uniform float u_float;
        uniform int u_int;
        uniform uint u_uint;
        uniform vec2 u_vec2;
        uniform vec3 u_vec3;
        uniform vec4 u_vec4;
        uniform mat4 u_mat4;
        uniform vec4 u_color;
        out vec4 out_color;
        void main() {
            bool ok = u_float == 0.5 && u_int == -3 && u_uint == 4000000000u
                      && u_vec2 == vec2(1.0, 2.0) && u_vec3 == vec3(3.0, 4.0, 5.0)
                      && u_vec4 == vec4(6.0, 7.0, 8.0, 9.0)
                      && u_mat4[3] == vec4(1.0, 2.0, 3.0, 1.0) && u_mat4[0] == vec4(2.0, 0.0, 0.0, 0.0)
                      && u_color == vec4(1.0, 0.0, 1.0, 1.0);
            out_color = ok ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
        }
    )");
    shader.set(shader.uniform<float>("u_float"), 0.5f);
    shader.set(shader.uniform<std::int32_t>("u_int"), -3);
    shader.set(shader.uniform<std::uint32_t>("u_uint"), 4000000000u);
    shader.set(shader.uniform<tgx::Vec2>("u_vec2"), {1, 2});
    shader.set(shader.uniform<tgx::Vec3>("u_vec3"), {3, 4, 5});
    shader.set(shader.uniform<tgx::Vec4>("u_vec4"), {6, 7, 8, 9});
    shader.set(shader.uniform<tgx::Mat4>("u_mat4"), tgx::translate({1, 2, 3}) * tgx::scale({2, 1, 1}));
    shader.set(shader.uniform<tgx::Color>("u_color"), tgx::colors::magenta);

    tgx_test::app().device().draw(shader, cover.vertices, {.target = &target});
    CHECK(target.read()[4, 4] == tgx::colors::green);
}

TEST_CASE("a uniform set on another shader between two draws") {
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    const auto cover = tgx_test::Cover::create();
    const tgx::gl::Shader red = colored(tgx::colors::red);
    tgx::gl::Shader other = colored(tgx::colors::blue);
    auto &device = tgx_test::app().device();

    device.draw(red, cover.vertices, {.target = &target});
    // GL 3.3 sets a uniform on the current program: other is made current.
    other.set(other.uniform<tgx::Color>("u_color"), tgx::colors::green);
    device.draw(red, cover.vertices, {.target = &target});
    CHECK(target.read()[4, 4] == tgx::colors::red);

    device.draw(other, cover.vertices, {.target = &target});
    CHECK(target.read()[4, 4] == tgx::colors::green);
}
