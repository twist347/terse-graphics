// An endless 80s drive at dusk: a striped sun sinking behind wireframe
// mountains, palms swaying, a neon grid rushing towards you. The scene is
// drawn into a render target the window's size; its bright parts, blurred
// into a smaller target, add the bloom, and a shader of its own adds
// scanlines, a vignette and a touch of color fringing on the way out.
//
// Left/right or the mouse: steer. Up/down: speed. C: the CRT effect on/off.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numbers>
#include <print>
#include <string>

namespace {
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

    // The light above a threshold, blurred over a few texels: the bloom.
    constexpr const char *bright_fragment = TGX_GLSL_VERSION R"(
        in vec2 uv;
        in vec4 color;
        uniform sampler2D u_texture;
        uniform vec2 u_texel;
        out vec4 out_color;
        void main() {
            vec3 sum = vec3(0.0);
            for (int i = 0; i < 25; ++i) {
                ivec2 at = ivec2(i % 5 - 2, i / 5 - 2);
                vec3 c = texture(u_texture, uv + vec2(at) * u_texel).rgb;
                sum += max(c - 0.45, 0.0) * (3.0 - 0.5 * float(abs(at.x) + abs(at.y)));
            }
            out_color = vec4(sum / 50.0, 1.0) * color;
        }
    )";

    // Scanlines every third pixel, color fringes towards the edges, dark corners.
    constexpr const char *crt_fragment = TGX_GLSL_VERSION R"(
        in vec2 uv;
        in vec4 color;
        uniform sampler2D u_texture;
        out vec4 out_color;
        void main() {
            vec2 from_middle = uv - 0.5;
            vec2 shift = from_middle * 0.006;
            vec3 rgb = texture(u_texture, uv).rgb;
            rgb.rb = vec2(texture(u_texture, uv + shift).r, texture(u_texture, uv - shift).b);
            float scanline = mix(1.0, 0.78, step(2.0, mod(gl_FragCoord.y, 3.0)));
            float vignette = 1.0 - dot(from_middle, from_middle) * 0.9;
            out_color = vec4(rgb * scanline * vignette, 1.0) * color;
        }
    )";

    constexpr float pi = std::numbers::pi_v<float>;
    // Fractions of the picture's height.
    constexpr float horizon_at = 0.6f;
    constexpr float sun_radius = 0.27f;
    // Grid rows are one unit apart in depth, the farthest at grid_far.
    constexpr float grid_far = 36.f;
    constexpr int grid_columns = 48;
    constexpr std::size_t ridge_points = 34;

    constexpr std::array sky{
        tgx::Color::rgb(0x06021a), tgx::Color::rgb(0x170636), tgx::Color::rgb(0x380c58),
        tgx::Color::rgb(0x6c1872), tgx::Color::rgb(0xb02e70), tgx::Color::rgb(0xec5a68),
    };
    constexpr tgx::Color sun_top = tgx::Color::rgb(0xffe27a);
    constexpr tgx::Color sun_bottom = tgx::Color::rgb(0xff3a8c);
    constexpr tgx::Color floor_far = tgx::Color::rgb(0x2a0838);
    constexpr tgx::Color floor_near = tgx::Color::rgb(0x05010c);
    constexpr tgx::Color grid_color = tgx::Color::rgb(0xff38d2);
    constexpr tgx::Color neon_cyan = tgx::Color::rgb(0x34f0ff);
    constexpr tgx::Color far_edge = tgx::Color::rgb(0xb05cff);
    constexpr tgx::Color far_fill = tgx::Color::rgb(0x1c0838);
    constexpr tgx::Color near_fill = tgx::Color::rgb(0x0c0322);
    constexpr tgx::Color shade = tgx::Color::rgb(0x0b0218);

    tgx::Random rng{19860707};

    // Peaks as fractions of the width and height, low in the middle for the sun.
    using Ridge = std::array<tgx::Vec2, ridge_points>;

    [[nodiscard]] auto make_ridge(float tallest) -> Ridge {
        Ridge ridge{};
        for (std::size_t i = 0; i < ridge_points; ++i) {
            const float x = -0.3f + 1.6f * static_cast<float>(i) / static_cast<float>(ridge_points - 1);
            const float side = std::min(std::abs(x - 0.5f) * 2.4f, 1.f);
            const float peak = i % 2 == 0 ? rng.next_float(0.45f, 1.f) : rng.next_float(0.15f, 0.5f);
            ridge[i] = {x + rng.next_float(-0.012f, 0.012f), tallest * peak * (0.12f + 0.88f * side * side)};
        }
        return ridge;
    }

    // A line twice, wide and dim then thin and bright, on an additive canvas.
    auto neon(tgx::Canvas &glow, tgx::Vec2 a, tgx::Vec2 b, tgx::Color color, float width, float alpha) -> void {
        glow.line(a, b, color.fade(0.14f * alpha), width * 5.f);
        glow.line(a, b, color.fade(alpha), width);
    }

    auto draw_ridge(tgx::Canvas &world, tgx::Canvas &glow, const Ridge &ridge, tgx::Vec2 view, float horizon,
                    float shift, tgx::Color edge, tgx::Color fill) -> void {
        Ridge top{};
        for (std::size_t i = 0; i < ridge_points; ++i) {
            top[i] = {ridge[i].x * view.x + shift, horizon - ridge[i].y * view.y};
        }
        const tgx::Color base = tgx::lerp(fill, sky.back(), 0.25f);
        for (std::size_t i = 0; i + 1 < ridge_points; ++i) {
            const tgx::Vec2 a{top[i].x, horizon};
            const tgx::Vec2 b{top[i + 1].x, horizon};
            world.triangle_gradient(top[i], top[i + 1], b, fill, fill, base);
            world.triangle_gradient(top[i], b, a, fill, base, base);
            // The wireframe: down from each peak, across, and halfway up.
            glow.line(top[i], a, edge.fade(0.35f), 1.f);
            glow.line(top[i], b, edge.fade(0.25f), 1.f);
            glow.line(tgx::lerp(top[i], a, 0.5f), tgx::lerp(top[i + 1], b, 0.5f), edge.fade(0.3f), 1.f);
        }
        glow.line_strip(top, edge.fade(0.12f), 7.f);
        glow.line_strip(top, edge, 2.f);
    }

    // A bent trunk, thinner towards the top, and drooping leaves in the wind.
    auto draw_palm(tgx::Canvas &world, tgx::Vec2 base, float height, float lean, float time) -> void {
        const float sway = 0.04f * std::sin(time * 0.9f + base.x * 0.01f);
        const auto trunk = [&](float t) { return base + tgx::Vec2{lean * height * t * t, -height * t}; };
        for (float t = 0.f; t < 1.f; t += 0.05f) {
            world.line(trunk(t), trunk(t + 0.06f), shade, height * (0.045f - 0.025f * t));
        }
        const tgx::Vec2 crown = trunk(1.f);
        for (int i = 0; i < 9; ++i) {
            const float angle = -3.5f + 0.48f * static_cast<float>(i) + sway * static_cast<float>(1 + i % 3);
            const float length = height * (i % 2 == 0 ? 0.42f : 0.34f);
            const tgx::Vec2 out = tgx::from_angle(angle);
            const auto leaf = [&](float s) { return crown + (out + tgx::Vec2{0.f, 0.6f * s}) * length * s; };
            const auto width = [&](float s) { return length * 0.08f * std::sin(pi * s) + 1.f; };
            for (float s = 0.f; s < 1.f; s += 0.125f) {
                const tgx::Vec2 a = leaf(s);
                const tgx::Vec2 b = leaf(s + 0.125f);
                const tgx::Vec2 n = tgx::perpendicular(tgx::normalize(b - a));
                world.triangle(a + n * width(s), b + n * width(s + 0.125f), b - n * width(s + 0.125f), shade);
                world.triangle(a + n * width(s), b - n * width(s + 0.125f), a - n * width(s), shade);
            }
        }
    }

    auto draw_sun(tgx::Canvas &world, tgx::Canvas &glow, tgx::Vec2 view, float horizon, float time) -> void {
        const float r = sun_radius * view.y;
        const tgx::Vec2 center{view.x / 2.f, horizon - r * 0.72f};
        glow.circle_gradient(center, r * 2.4f, sun_bottom.fade(0.3f), sun_bottom.fade(0.f));
        glow.circle_gradient(center, r * 1.4f, sun_top.fade(0.2f), sun_top.fade(0.f));
        // From y0 to y1 in rows of trapezoids along the edge.
        const auto color_at = [&](float y) { return tgx::lerp(sun_top, sun_bottom, (y - center.y + r) / (2.f * r)); };
        const auto half = [&](float y) { return std::sqrt(std::max(r * r - (y - center.y) * (y - center.y), 0.f)); };
        const auto slab = [&](float y0, float y1) {
            const float rows = std::ceil((y1 - y0) / 2.f);
            for (float row = 0.f; row < rows; row += 1.f) {
                const float ya = y0 + (y1 - y0) * row / rows;
                const float yb = y0 + (y1 - y0) * (row + 1.f) / rows;
                const float wa = half(ya);
                const float wb = half(yb);
                const tgx::Color ca = color_at(ya);
                const tgx::Color cb = color_at(yb);
                world.triangle_gradient({center.x - wa, ya}, {center.x + wa, ya}, {center.x + wb, yb}, ca, ca, cb);
                world.triangle_gradient({center.x - wa, ya}, {center.x + wb, yb}, {center.x - wb, yb}, ca, cb, cb);
            }
        };
        // Solid between gaps that widen and slide down; the glow shows through.
        constexpr int gaps = 7;
        float y = center.y - r;
        for (int i = 0; i <= gaps; ++i) {
            const float v = (static_cast<float>(i) + std::fmod(time * 0.18f, 1.f)) / gaps;
            const float gap = i < gaps ? center.y - r * 0.1f + v * (horizon - center.y + r * 0.1f) : horizon;
            const float thick = r * (0.015f + 0.11f * v * v) * std::min(v * 6.f, 1.f);
            // One too thin to see would only flicker.
            if (i == gaps || thick >= 1.f) {
                slab(y, std::min(gap, horizon));
                y = gap + thick;
            }
        }
    }

    // Rows nearer brighter and thicker, columns all meeting at the middle of
    // the horizon. lateral is the sideways position, in columns.
    auto draw_grid(tgx::Canvas &glow, tgx::Vec2 view, float horizon, float travel, float lateral) -> void {
        const float focal = (view.y - horizon) * 0.55f;
        const float offset = std::fmod(travel, 1.f);
        for (float z = 1.f - offset; z < grid_far; z += 1.f) {
            const float y = horizon + focal / z;
            const float fade = std::pow(1.f - z / grid_far, 2.f);
            neon(glow, {-view.x * 0.2f, y}, {view.x * 1.2f, y}, grid_color, std::clamp(2.6f / z, 1.f, 3.f), fade);
        }
        constexpr int half_columns = grid_columns / 2;
        const float first = std::floor(lateral) - static_cast<float>(half_columns);
        for (int k = 0; k <= grid_columns; ++k) {
            const float x = first + static_cast<float>(k) - lateral;
            const tgx::Vec2 far{view.x / 2.f + focal * x / grid_far, horizon + focal / grid_far};
            const tgx::Vec2 near{view.x / 2.f + focal * x / 0.25f, horizon + focal / 0.25f};
            neon(glow, far, near, grid_color, 1.6f, 0.85f);
        }
    }
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - scenes 04 synthwave"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    std::string log;
    auto bright = tgx::gl::Shader::from_source(vertex_source, bright_fragment, &log);
    auto crt = tgx::gl::Shader::from_source(vertex_source, crt_fragment, &log);
    if (!bright || !crt) {
        std::print(stderr, "shader: {}\n{}", !bright ? bright.error() : crt.error(), log);
        return 1;
    }
    const auto u_texel = bright->uniform<tgx::Vec2>("u_texel");

    // The scene at the window's pixels, its bloom at a quarter of them; made
    // again when the window's size changes.
    tgx::Size size{1, 1};
    auto scene = tgx::RenderTarget::create(size);
    auto bloom = tgx::RenderTarget::create(size);

    // Into the scene: shapes, and light adding up. Out: bloom, then window.
    auto &canvas = app->canvas();
    tgx::Canvas world = canvas;
    tgx::Canvas light = canvas;
    light.set_blend(tgx::Blend::additive);
    tgx::Canvas glow = light;
    tgx::Canvas blur = canvas;
    blur.set_shader(&*bright);
    blur.set_blend(tgx::Blend::none);
    tgx::Canvas tube = canvas;
    tube.set_blend(tgx::Blend::premultiplied);

    const std::array ridges{make_ridge(0.26f), make_ridge(0.17f)};
    std::array<tgx::Vec2, 160> stars{};
    for (tgx::Vec2 &star: stars) {
        star = {rng.next_float(-0.1f, 1.1f), rng.next_float(0.f, 1.f)};
    }
    float travel = 0.f;
    float speed = 3.f;
    float steer = 0.f;
    float steer_target = 0.f;
    float bank = 0.f;
    bool crt_on = true;

    while (!app->should_close()) {
        app->poll_events();
        const auto &input = app->input();
        if (app->window().framebuffer_size() != size && !app->window().framebuffer_size().empty()) {
            size = app->window().framebuffer_size();
            scene = tgx::RenderTarget::create(size);
            bloom = tgx::RenderTarget::create({std::max(size.width / 4, 1), std::max(size.height / 4, 1)});
        }
        if (!scene || !bloom) {
            std::println(stderr, "render target: {}", !scene ? scene.error() : bloom.error());
            return 1;
        }
        world.set_target(&*scene);
        glow.set_target(&*scene);
        blur.set_target(&*bloom);

        const float dt = std::min(app->clock().delta(), 0.1f);
        const float time = static_cast<float>(app->clock().elapsed());
        // Keys turn the wheel, the mouse sets it; the view follows and banks.
        const tgx::Vec2 keys = input.direction(tgx::Key::left, tgx::Key::right, tgx::Key::up, tgx::Key::down);
        steer_target = std::clamp(steer_target + keys.x * 1.6f * dt, -1.f, 1.f);
        if (tgx::length_squared(input.mouse_delta()) > 0.f && canvas.size().width > 0) {
            steer_target = std::clamp(input.mouse().x / static_cast<float>(canvas.size().width) * 2.f - 1.f, -1.f, 1.f);
        }
        const float turn = (steer_target - steer) * std::min(dt * 3.f, 1.f);
        steer += turn;
        bank += (turn / std::max(dt, 1e-4f) - bank) * std::min(dt * 6.f, 1.f);
        speed = std::clamp(speed - keys.y * 4.f * dt, 0.5f, 12.f);
        travel += speed * dt;
        crt_on = crt_on != input.pressed(tgx::Key::c);
        tube.set_shader(crt_on ? &*crt : nullptr);

        const tgx::Vec2 view{static_cast<float>(size.width), static_cast<float>(size.height)};
        const float horizon = view.y * horizon_at;
        const tgx::Camera2D camera{.target = view / 2.f, .offset = view / 2.f, .rotation = -bank * 0.05f};
        world.set_camera(camera);
        glow.set_camera(camera);

        // Wider and taller than the picture, so the bank shows no edges.
        const tgx::Rect wide{-view.x * 0.1f, -view.y * 0.2f, view.x * 1.2f, view.y * 1.4f};
        const float step = horizon / static_cast<float>(sky.size() - 1);
        world.rect({wide.x, wide.y, wide.width, -wide.y}, sky[0]);
        for (std::size_t i = 0; i + 1 < sky.size(); ++i) {
            world.rect_gradient({wide.x, step * static_cast<float>(i), wide.width, step}, sky[i], sky[i + 1]);
        }
        for (std::size_t i = 0; i < stars.size(); ++i) {
            const tgx::Vec2 p{(stars[i].x - steer * 0.01f) * view.x, stars[i].y * stars[i].y * horizon * 0.75f};
            const float twinkle = 0.6f + 0.4f * std::sin(time * (1.f + stars[i].x * 3.f) + static_cast<float>(i));
            glow.rect({p.x, p.y, 1.5f, 1.5f}, tgx::colors::white.fade((1.f - stars[i].y * stars[i].y) * twinkle));
        }

        draw_sun(world, glow, view, horizon, time);
        const float shift = -steer * view.x * 0.06f;
        draw_ridge(world, glow, ridges[0], view, horizon, shift * 0.5f, far_edge, far_fill);
        draw_ridge(world, glow, ridges[1], view, horizon, shift, neon_cyan, near_fill);

        // The floor, its grid, a haze, the sun on it, a line along the horizon.
        world.rect_gradient({wide.x, horizon, wide.width, wide.bottom() - horizon}, floor_far, floor_near);
        draw_grid(glow, view, horizon, travel, steer * 4.f);
        world.rect_gradient({wide.x, horizon, wide.width, (view.y - horizon) * 0.3f}, floor_far, floor_far.fade(0.f));
        glow.circle_gradient({view.x / 2.f, horizon}, view.y * 0.45f, sun_bottom.fade(0.16f), sun_bottom.fade(0.f));
        neon(glow, {wide.x, horizon}, {wide.right(), horizon}, tgx::Color::rgb(0xff9ad8), 1.5f, 1.f);

        draw_palm(world, {view.x * 0.07f + shift * 2.5f, view.y + 10.f}, view.y * 0.66f, 0.28f, time);
        draw_palm(world, {view.x * 0.95f + shift * 2.5f, view.y + 10.f}, view.y * 0.56f, -0.32f, time);

        // The bright parts into the bloom; the scene, the bloom, the words.
        bright->set(u_texel, tgx::Vec2{2.f / view.x, 2.f / view.y});
        blur.sprite(scene->texture(), {.size = view / 4.f});
        const tgx::Vec2 screen{static_cast<float>(canvas.size().width), static_cast<float>(canvas.size().height)};
        canvas.clear(tgx::colors::black);
        tube.sprite(scene->texture(), {.size = screen});
        light.sprite(bloom->texture(), {.size = screen, .tint = tgx::colors::white.fade(0.9f)});

        canvas.text({16.f, 50.f}, "NEON DRIVE", tgx::Color::rgb(0xffd0f4), 32.f);
        canvas.text({16.f, screen.y - 28.f}, "left/right, mouse: steer   up/down: speed   c: crt", neon_cyan);

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
