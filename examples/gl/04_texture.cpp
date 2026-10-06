// A texture on a square. Each vertex carries a uv, a point of the image from
// (0, 0) at its top-left to (1, 1) at its bottom-right; the shader reads the
// texture there through a sampler, and the draw says which texture that is.

#include "tgx/tgx.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <print>
#include <string>

namespace {
    struct Vertex {
        tgx::Vec2 position;
        tgx::Vec2 uv;
    };

    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec2 in_uv;

        uniform mat4 u_transform;

        out vec2 uv;

        void main() {
            gl_Position = u_transform * vec4(in_position, 0.0, 1.0);
            uv = in_uv;
        }
    )";

    constexpr const char *fragment_source = TGX_GLSL_VERSION R"(
        in vec2 uv;

        uniform sampler2D u_texture;

        out vec4 out_color;

        void main() {
            out_color = texture(u_texture, uv);
        }
    )";

    // The image's top-left corner on the square's top-left corner.
    constexpr std::array vertices{
        Vertex{{-0.8f, 0.8f}, {0, 0}},
        Vertex{{0.8f, 0.8f}, {1, 0}},
        Vertex{{0.8f, -0.8f}, {1, 1}},
        Vertex{{-0.8f, -0.8f}, {0, 1}},
    };

    constexpr std::array<std::uint16_t, 6> indices{
        0, 1, 2,
        0, 2, 3,
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
        tgx::gl::VertexAttribute::of(1, &Vertex::uv),
    };
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "gl - 04 texture"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    std::string log;
    auto shader = tgx::gl::Shader::from_source(vertex_source, fragment_source, &log);
    if (!shader) {
        std::print(stderr, "shader: {}\n{}", shader.error(), log);
        return 1;
    }

    const auto u_transform = shader->uniform<tgx::Mat4>("u_transform");
    // A sampler is set to a slot, not to a texture: it reads whatever texture
    // the draw puts in slot 0.
    shader->set(shader->uniform<tgx::gl::TextureSlot>("u_texture"), {0});

    // An 8x8 checkerboard with a red top-left pixel, to see which way up it
    // lands.
    auto image = tgx::Image::create({8, 8});
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            image[x, y] = (x + y) % 2 == 0 ? tgx::colors::white : tgx::colors::gray;
        }
    }
    image[0, 0] = tgx::colors::red;

    auto texture = tgx::Texture::create(image, {.filter = tgx::TextureFilter::nearest});
    if (!texture) {
        std::println(stderr, "texture: {}", texture.error());
        return 1;
    }

    auto vbo = tgx::gl::Buffer<Vertex>::create(vertices);
    auto ibo = tgx::gl::Buffer<std::uint16_t>::create(indices);
    if (!vbo || !ibo) {
        std::println(stderr, "buffer: {}", !vbo ? vbo.error() : ibo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(layout);
    vao.set_vertex_buffer(*vbo);
    vao.set_index_buffer(*ibo);

    while (!app->should_close()) {
        app->poll_events();

        // Keeps the square square, as in 03_uniforms.
        const float aspect = app->window().framebuffer_size().aspect();
        shader->set(u_transform, tgx::scale({1 / aspect, 1, 1}));

        app->device().clear({.color = tgx::colors::dark_gray});
        // textures[0] is slot 0.
        app->device().draw(*shader, vao, {.textures = {&*texture}});

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
