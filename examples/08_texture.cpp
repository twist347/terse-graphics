#include "tgx/tgx.h"

#include <array>
#include <cstdio>
#include <format>
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

        uniform mat4 u_projection;

        out vec2 uv;

        void main() {
            gl_Position = u_projection * vec4(in_position, 0.0, 1.0);
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

    // A square of two triangles. uv (0, 0) is the image's top-left pixel, so it
    // goes on the top-left corner.
    constexpr std::array vertices{
        Vertex{{-0.8f, 0.8f}, {0.f, 0.f}},
        Vertex{{0.8f, 0.8f}, {1.f, 0.f}},
        Vertex{{0.8f, -0.8f}, {1.f, 1.f}},

        Vertex{{-0.8f, 0.8f}, {0.f, 0.f}},
        Vertex{{0.8f, -0.8f}, {1.f, 1.f}},
        Vertex{{-0.8f, -0.8f}, {0.f, 1.f}},
    };

    const std::array layout{
        tgx::gl::VertexAttribute::of(0, &Vertex::position),
        tgx::gl::VertexAttribute::of(1, &Vertex::uv),
    };

    constexpr const char *title = "tgx - 08 texture";

    // An 8x8 checkerboard with a red top-left pixel, so it is plain which way
    // up the image lands.
    [[nodiscard]] auto make_image() -> tgx::Image {
        auto image = tgx::Image::create({8, 8});
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                image.at(x, y) = (x + y) % 2 == 0 ? tgx::colors::light_gray : tgx::colors::dark_gray;
            }
        }
        image.at(0, 0) = tgx::colors::red;
        return image;
    }

    // As in 04_circle: 2 units tall and as wide as the window's aspect ratio,
    // so the square stays square.
    [[nodiscard]] constexpr auto projection_for(tgx::Size size) noexcept -> tgx::Mat4 {
        const float aspect = size.aspect();
        return tgx::ortho(-aspect, aspect, -1.f, 1.f);
    }
}

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = title});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    std::string log;
    auto shader = tgx::gl::Shader::from_source(app->device(), vertex_source, fragment_source, &log);
    if (!shader) {
        std::print(stderr, "shader: {}\n{}", shader.error(), log);
        return 1;
    }

    const auto u_projection = shader->uniform<tgx::Mat4>("u_projection");
    shader->set(u_projection, projection_for(app->window().framebuffer_size()));
    // The sampler reads slot 0 of every draw.
    shader->set(shader->uniform<tgx::gl::TextureSlot>("u_texture"), {0});

    // Nearest keeps the 8 pixels as sharp squares; linear would blur them.
    auto texture = tgx::gl::Texture::create(app->device(), make_image(), {.filter = tgx::gl::TextureFilter::nearest});
    if (!texture) {
        std::println(stderr, "texture: {}", texture.error());
        return 1;
    }

    auto vbo = tgx::gl::Buffer::create(app->device(), vertices);
    if (!vbo) {
        std::println(stderr, "buffer: {}", vbo.error());
        return 1;
    }

    auto vao = tgx::gl::VertexArray::create<Vertex>(app->device(), layout);
    vao.set_vertex_buffer(*vbo);

    app->device().set_clear_color(tgx::colors::black);

    while (!app->should_close()) {
        app->poll_events();

        if (app->clock().fps_updated()) {
            app->window().set_title(std::format("{} - {:.0f} fps", title, app->clock().fps()).c_str());
        }

        if (app->resized()) {
            shader->set(u_projection, projection_for(app->window().framebuffer_size()));
        }

        app->device().clear();
        app->device().draw(*shader, vao, {.textures = {&*texture}});

        app->swap_buffers();
    }

    return 0;
}
