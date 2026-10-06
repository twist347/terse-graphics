// A whole frame through a shader: the scene is drawn into a render target,
// then onto the window by one triangle covering it, whose shader reads the
// target and darkens its edges. Hold Space to see the scene as it was drawn.
//
// The scene is two triangles that pass through each other: the target has a
// depth buffer, so wherever they overlap, the nearer one is in front.

#include "tgx/tgx.h"

#include <array>
#include <cstdio>
#include <print>
#include <string>
#include <utility>

namespace {
    struct SceneVertex {
        tgx::Vec3 position;
        tgx::Color color;
    };

    struct CoverVertex {
        tgx::Vec2 position;
    };

    constexpr const char *scene_vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec3 in_position;
        layout(location = 1) in vec4 in_color;

        out vec4 color;

        void main() {
            gl_Position = vec4(in_position, 1.0);
            color = in_color;
        }
    )";

    constexpr const char *scene_fragment_source = TGX_GLSL_VERSION R"(
        in vec4 color;

        out vec4 out_color;

        void main() {
            out_color = color;
        }
    )";

    // In clip space, where a smaller z is nearer. The red one points up and
    // leans: nearer on the left, further on the right. The blue one points
    // down and stands straight. Left of the middle red is in front, right of
    // it blue.
    constexpr std::array scene{
        SceneVertex{{-0.7f, -0.6f, -0.6f}, tgx::colors::red},
        SceneVertex{{0.7f, -0.6f, 0.6f}, tgx::colors::red},
        SceneVertex{{0.f, 0.7f, 0.f}, tgx::colors::orange},
        SceneVertex{{-0.7f, 0.6f, 0.f}, tgx::colors::blue},
        SceneVertex{{0.7f, 0.6f, 0.f}, tgx::colors::blue},
        SceneVertex{{0.f, -0.7f, 0.f}, tgx::colors::cyan},
    };

    // One triangle big enough to cover the window: the corners past it are
    // cut off. Its uv runs 0 to 1 across the window from the bottom-left,
    // as the target's rows do (Texture::bottom_up()): GL drew them that way.
    constexpr const char *post_vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;

        out vec2 uv;

        void main() {
            gl_Position = vec4(in_position, 0.0, 1.0);
            uv = in_position * 0.5 + 0.5;
        }
    )";

    // Darker towards the corners, and red and blue pulled a little apart
    // there, as a cheap lens does. u_strength 0 leaves the scene as it is.
    constexpr const char *post_fragment_source = TGX_GLSL_VERSION R"(
        in vec2 uv;

        uniform sampler2D u_scene;
        uniform float u_strength;

        out vec4 out_color;

        void main() {
            vec2 from_middle = uv - 0.5;
            vec2 shift = from_middle * 0.02 * u_strength;
            float r = texture(u_scene, uv + shift).r;
            float g = texture(u_scene, uv).g;
            float b = texture(u_scene, uv - shift).b;
            float vignette = 1.0 - dot(from_middle, from_middle) * 1.8 * u_strength;
            out_color = vec4(vec3(r, g, b) * vignette, 1.0);
        }
    )";

    constexpr std::array cover{CoverVertex{{-1, -1}}, CoverVertex{{3, -1}}, CoverVertex{{-1, 3}}};

    // As big as the window's framebuffer, pixel for pixel.
    [[nodiscard]] auto make_target(tgx::Size size) -> tgx::Result<tgx::RenderTarget> {
        return tgx::RenderTarget::create(size, {.depth = true});
    }
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "gl - 08 post process"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    std::string log;
    auto scene_shader = tgx::gl::Shader::from_source(scene_vertex_source, scene_fragment_source, &log);
    if (!scene_shader) {
        std::print(stderr, "scene shader: {}\n{}", scene_shader.error(), log);
        return 1;
    }
    auto post_shader = tgx::gl::Shader::from_source(post_vertex_source, post_fragment_source, &log);
    if (!post_shader) {
        std::print(stderr, "post shader: {}\n{}", post_shader.error(), log);
        return 1;
    }
    post_shader->set(post_shader->uniform<tgx::gl::TextureSlot>("u_scene"), {0});
    const auto u_strength = post_shader->uniform<float>("u_strength");

    auto scene_vbo = tgx::gl::Buffer<SceneVertex>::create(scene);
    auto cover_vbo = tgx::gl::Buffer<CoverVertex>::create(cover);
    if (!scene_vbo || !cover_vbo) {
        std::println(stderr, "buffer: {}", !scene_vbo ? scene_vbo.error() : cover_vbo.error());
        return 1;
    }

    auto scene_vao = tgx::gl::VertexArray::create<SceneVertex>(std::array{
        tgx::gl::VertexAttribute::of(0, &SceneVertex::position),
        tgx::gl::VertexAttribute::of(1, &SceneVertex::color),
    });
    scene_vao.set_vertex_buffer(*scene_vbo);
    auto cover_vao = tgx::gl::VertexArray::create<CoverVertex>(std::array{
        tgx::gl::VertexAttribute::of(0, &CoverVertex::position),
    });
    cover_vao.set_vertex_buffer(*cover_vbo);

    auto target = make_target(app->window().framebuffer_size());
    if (!target) {
        std::println(stderr, "render target: {}", target.error());
        return 1;
    }

    auto &device = app->device();
    while (!app->should_close()) {
        app->poll_events();

        // A target's size is fixed: a new one for the new framebuffer. A
        // minimized window has none to make, so the old one stays.
        const tgx::Size size = app->window().framebuffer_size();
        if (app->window().resized() && !size.empty()) {
            if (auto resized = make_target(size)) {
                target = std::move(resized);
            }
        }

        device.clear({.target = &*target, .color = tgx::colors::dark_gray, .depth = 1.f});
        device.draw(*scene_shader, scene_vao, {
            .target = &*target,
            .state = {.depth = tgx::gl::Depth::less},
        });

        post_shader->set(u_strength, app->input().down(tgx::Key::space) ? 0.f : 1.f);
        device.draw(*post_shader, cover_vao, {.textures = {&target->texture()}});

        app->canvas().fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
