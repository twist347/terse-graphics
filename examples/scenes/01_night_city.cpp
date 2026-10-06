// A rainy night over a city, in pixel art. Everything is drawn into a
// 320x180 render target, then scaled up onto the window through a shader of
// its own that adds scanlines and a vignette. Three layers of buildings move
// at different speeds as the view drifts along the street; rain falls and
// splashes on the road; street lamps and a passing car light it up.
//
// The mouse carries a lantern; Space calls lightning, which also comes on its
// own now and then; F12 saves a screenshot.

#include "tgx/gl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace {
    constexpr tgx::Size resolution{320, 180};
    constexpr float width = static_cast<float>(resolution.width);
    constexpr float height = static_cast<float>(resolution.height);
    // Where the street begins.
    constexpr float road_y = 160.f;

    // Seeded: the same city on every run and every platform.
    tgx::Random rng{20261005};

    // The scanlines and the vignette, on top of the canvas's own drawing: one
    // scanline per row of the target's pixels, darker where a row meets the
    // next, as on an old screen, and red and blue pulled a little apart
    // towards the edges.
    constexpr const char *crt_vertex = TGX_GLSL_VERSION R"(
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

    constexpr const char *crt_fragment = TGX_GLSL_VERSION R"(
        in vec2 uv;
        in vec4 color;

        uniform sampler2D u_texture;
        // The picture's rows, one scanline each.
        uniform float u_rows;

        out vec4 out_color;

        void main() {
            vec2 from_middle = uv - 0.5;
            vec2 shift = from_middle * 0.004;
            vec3 rgb = vec3(
                texture(u_texture, uv + shift).r,
                texture(u_texture, uv).g,
                texture(u_texture, uv - shift).b
            );
            float row = fract(uv.y * u_rows);
            float scanline = mix(1.0, 0.7, smoothstep(0.25, 0.5, abs(row - 0.5)));
            float vignette = 1.0 - dot(from_middle, from_middle) * 1.2;
            out_color = vec4(rgb * scanline * vignette, 1.0) * color;
        }
    )";

    // A window of a building; a lit one flickers off now and then, and back.
    struct Pane {
        tgx::Rect rect;
        bool lit;
    };

    struct Building {
        tgx::Rect rect;
        std::vector<Pane> panes;
    };

    // A band of buildings twice as wide as the view, drawn twice side by side
    // so it can scroll for ever. speed is how fast it moves against the near
    // street: the far ones slower.
    struct Layer {
        std::vector<Building> buildings;
        float speed;
        tgx::Color wall;
        tgx::Color window;
        float span;
    };

    // lit is the share of windows lit at the start.
    [[nodiscard]] auto make_layer(
        float speed,
        float min_height,
        float max_height,
        tgx::Color wall,
        tgx::Color window,
        float lit
    ) -> Layer {
        Layer layer{{}, speed, wall, window, width * 2.f};
        for (float x = 0.f; x < layer.span;) {
            // The last one ends where the next copy of the band begins.
            const float w = std::min(std::round(rng.next_float(18.f, 46.f)), layer.span - x);
            const float h = std::round(rng.next_float(min_height, max_height));
            Building building{{x, road_y - h, w, h}, {}};
            // Windows of 2x3 pixels in a grid, inset from the walls.
            for (float wy = road_y - h + 4.f; wy < road_y - 6.f; wy += 6.f) {
                for (float wx = x + 3.f; wx + 2.f < x + w - 2.f; wx += 5.f) {
                    building.panes.push_back({{wx, wy, 2.f, 3.f}, rng.chance(lit)});
                }
            }
            layer.buildings.push_back(std::move(building));
            x += w + std::round(rng.next_float(0.f, 6.f));
        }
        return layer;
    }

    struct Drop {
        tgx::Vec2 position;
        float speed;
    };

    // How long a splash lasts, in seconds.
    constexpr float splash_life = 0.3f;

    struct Splash {
        tgx::Vec2 position;
        tgx::Vec2 velocity;
        float life;
    };

    // How long a strike lights the sky, in seconds, and the color of its glow.
    constexpr float strike_time = 0.35f;
    constexpr tgx::Color strike_glow = tgx::Color::rgb(0x9fb4ff);

    struct Lightning {
        std::vector<tgx::Vec2> bolt;
        float left{0.f};
    };

    // A jagged path from the sky down to a roof.
    [[nodiscard]] auto make_bolt() -> std::vector<tgx::Vec2> {
        std::vector<tgx::Vec2> bolt{{rng.next_float(40.f, width - 40.f), 0.f}};
        while (bolt.back().y < road_y - 60.f) {
            const tgx::Vec2 last = bolt.back();
            bolt.push_back({last.x + rng.next_float(-9.f, 9.f), last.y + rng.next_float(6.f, 14.f)});
        }
        return bolt;
    }

    // The sky from deep blue at the top to a city glow at the horizon.
    [[nodiscard]] auto sky_at(float y, float flash) -> tgx::Color {
        const tgx::Color sky = tgx::lerp(tgx::Color::rgb(0x0b1026), tgx::Color::rgb(0x3a2f5c), y / road_y);
        return tgx::lerp(sky, tgx::Color::rgb(0x9aa6d6), flash);
    }

    auto draw_layer(tgx::Canvas &canvas, const Layer &layer, float scroll, float time) -> void {
        const float offset = -std::fmod(scroll * layer.speed, layer.span);
        for (const float base: {offset, offset + layer.span}) {
            for (const Building &building: layer.buildings) {
                const tgx::Rect r = building.rect;
                if (r.x + base > width || r.right() + base < 0.f) {
                    continue;
                }
                canvas.rect({r.x + base, r.y, r.width, r.height}, layer.wall);
                for (const Pane &pane: building.panes) {
                    if (pane.lit) {
                        const tgx::Rect w = pane.rect;
                        // A slow shimmer, as of a television inside.
                        const float glow = 0.85f + 0.15f * std::sin(time * 3.f + w.x * 0.7f + w.y);
                        canvas.rect({w.x + base, w.y, w.width, w.height}, layer.window.fade(glow));
                    }
                }
            }
        }
    }

    // A pole, a lamp, its cone of light down to the wet road and the
    // light's reflection in it.
    auto draw_lamp(tgx::Canvas &canvas, tgx::Canvas &light, float x) -> void {
        const tgx::Color warm = tgx::Color::rgb(0xffd27a);
        const tgx::Color iron = tgx::Color::rgb(0x1a1a24);
        canvas.rect({x, road_y - 28.f, 1.f, 28.f}, iron);
        canvas.rect({x - 3.f, road_y - 29.f, 7.f, 2.f}, iron);
        canvas.rect({x - 2.f, road_y - 27.f, 5.f, 1.f}, warm);

        light.triangle({x - 1.f, road_y - 26.f}, {x - 10.f, road_y}, {x + 11.f, road_y}, warm.fade(0.07f));
        light.circle_gradient({x + 0.5f, road_y - 26.f}, 8.f, warm.fade(0.3f), warm.fade(0.f));
        light.rect({x - 2.f, road_y + 1.f, 5.f, 12.f}, warm.fade(0.18f));
    }

    auto draw_car(tgx::Canvas &canvas, tgx::Canvas &light, float x) -> void {
        const float y = road_y + 6.f;
        const tgx::Color glass = tgx::Color::rgb(0x2b3a55);
        const tgx::Color beam = tgx::Color::rgb(0xfff4c2);
        canvas.rect({x, y - 6.f, 26.f, 6.f}, tgx::Color::rgb(0xb8323f));
        canvas.rect({x + 6.f, y - 10.f, 13.f, 4.f}, tgx::Color::rgb(0x8f2632));
        canvas.rect({x + 8.f, y - 9.f, 4.f, 3.f}, glass);
        canvas.rect({x + 13.f, y - 9.f, 4.f, 3.f}, glass);
        canvas.circle({x + 6.f, y}, 2.5f, tgx::colors::black);
        canvas.circle({x + 20.f, y}, 2.5f, tgx::colors::black);
        // Driving to the left: headlights in front, tail lights behind.
        canvas.rect({x, y - 5.f, 1.f, 2.f}, beam);
        canvas.rect({x + 25.f, y - 5.f, 1.f, 2.f}, tgx::colors::red);

        light.triangle({x, y - 4.f}, {x - 60.f, y - 12.f}, {x - 60.f, y + 3.f}, beam.fade(0.16f));
        light.rect({x - 40.f, y + 2.f, 40.f, 3.f}, beam.fade(0.10f));
        light.circle({x + 25.5f, y - 4.f}, 3.f, tgx::colors::red.fade(0.25f));
    }
}

int main() {
    // Info also prints which GL context the driver gave.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "scenes - 01 night city"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto pixels = tgx::RenderTarget::create(resolution, {.filter = tgx::TextureFilter::nearest});
    if (!pixels) {
        std::println(stderr, "render target: {}", pixels.error());
        return 1;
    }

    std::string log;
    auto crt = tgx::gl::Shader::from_source(crt_vertex, crt_fragment, &log);
    if (!crt) {
        std::print(stderr, "crt shader: {}\n{}", crt.error(), log);
        return 1;
    }
    crt->set(crt->uniform<float>("u_rows"), static_cast<float>(resolution.height));

    // Three ways of drawing: the world into the target, the light into it
    // too but adding up, and the target onto the window through the shader.
    auto &screen = app->canvas();
    tgx::Canvas world = screen;
    world.set_target(&*pixels);
    tgx::Canvas light = world;
    light.set_blend(tgx::Blend::additive);
    tgx::Canvas tube = screen;
    tube.set_shader(&*crt);

    std::array layers{
        make_layer(0.15f, 50.f, 110.f, tgx::Color::rgb(0x1b1836), tgx::Color::rgb(0x5a5a8a), 0.10f),
        make_layer(0.45f, 35.f, 85.f, tgx::Color::rgb(0x120f24), tgx::Color::rgb(0xc9a25e), 0.25f),
        make_layer(1.00f, 20.f, 60.f, tgx::Color::rgb(0x07060f), tgx::Color::rgb(0xffcf73), 0.35f),
    };

    std::vector<tgx::Vec2> stars(90);
    for (tgx::Vec2 &star: stars) {
        star = {std::round(rng.next_float(0.f, width)), std::round(rng.next_float(0.f, 90.f))};
    }

    std::vector<Drop> drops(800);
    for (Drop &drop: drops) {
        drop = {rng.point_in({0.f, -height, width + 60.f, height + road_y}), rng.next_float(160.f, 240.f)};
    }
    std::vector<Splash> splashes;
    // Falling a little to the left, as in a wind.
    const tgx::Vec2 rain_direction = tgx::normalize(tgx::Vec2{-0.25f, 1.f});

    Lightning lightning;
    float next_strike = rng.next_float(4.f, 9.f);
    float car_x = width + 40.f;
    float scroll = 0.f;
    float time = 0.f;

    while (!app->should_close()) {
        app->poll_events();

        // A long pause (a dragged window) must not move everything at once.
        const float dt = std::min(app->clock().delta(), 0.1f);
        time += dt;
        scroll += 12.f * dt;

        // The picture scaled up by a whole factor, in the middle of the window.
        const tgx::Rect picture = tgx::fit_whole(resolution, screen.size());
        const tgx::Vec2 position{picture.x, picture.y};
        const tgx::Vec2 size{picture.width, picture.height};
        // A minimized window has no picture to point into.
        const float to_pixels = picture.width > 0.f ? width / picture.width : 0.f;
        const tgx::Vec2 mouse = (app->input().mouse() - position) * to_pixels;

        // Rain: drops fall along the wind and splash where they meet the road.
        for (Drop &drop: drops) {
            drop.position += rain_direction * drop.speed * dt;
            if (drop.position.y >= road_y + rng.next_float(0.f, 18.f)) {
                if (splashes.size() < 600 && rng.chance(0.6f)) {
                    for (int i = 0; i < 2; ++i) {
                        const tgx::Vec2 velocity = rng.point_in({-18.f, -40.f, 36.f, 20.f});
                        splashes.push_back({drop.position, velocity, splash_life});
                    }
                }
                drop.position = rng.point_in({0.f, -20.f, width + 60.f, 18.f});
            }
        }
        for (Splash &splash: splashes) {
            splash.velocity.y += 160.f * dt;
            splash.position += splash.velocity * dt;
            splash.life -= dt;
        }
        std::erase_if(splashes, [](const Splash &splash) { return splash.life <= 0.f; });

        // Windows go dark and light up again, a few a second.
        for (Layer &layer: layers) {
            for (Building &building: layer.buildings) {
                for (Pane &w: building.panes) {
                    if (rng.chance(0.02f * dt)) {
                        w.lit = !w.lit;
                    }
                }
            }
        }

        if (app->input().pressed(tgx::Key::space) || time > next_strike) {
            lightning = {make_bolt(), strike_time};
            next_strike = time + rng.next_float(6.f, 14.f);
        }
        lightning.left = std::max(lightning.left - dt, 0.f);
        // Two flickers, bright then fading.
        const float flash = lightning.left > 0.f
                                ? (std::fmod(lightning.left, 0.12f) > 0.05f ? 1.f : 0.4f) * (lightning.left / strike_time)
                                : 0.f;

        car_x -= 45.f * dt;
        if (car_x < -90.f) {
            car_x = width + rng.next_float(60.f, 300.f);
        }

        // The world, into the target.
        world.rect_gradient({0.f, 0.f, width, road_y}, sky_at(0.f, flash * 0.6f), sky_at(road_y, flash * 0.6f));
        for (std::size_t i = 0; i < stars.size(); ++i) {
            const float twinkle = 0.5f + 0.5f * std::sin(time * 2.f + static_cast<float>(i) * 1.7f);
            world.rect({stars[i].x, stars[i].y, 1.f, 1.f}, tgx::colors::white.fade(0.3f + 0.5f * twinkle));
        }
        const tgx::Color moonlight = tgx::Color::rgb(0xf3ecd2);
        world.circle({262.f, 34.f}, 11.f, moonlight);
        world.circle({257.f, 31.f}, 10.f, sky_at(31.f, flash * 0.6f));
        light.circle_gradient({262.f, 34.f}, 26.f, moonlight.fade(0.12f), moonlight.fade(0.f));

        if (lightning.left > 0.f && flash > 0.5f) {
            light.line_strip(lightning.bolt, strike_glow.fade(0.5f), 3.f);
            world.line_strip(lightning.bolt, tgx::colors::white, 1.f);
        }

        draw_layer(world, layers[0], scroll, time);
        draw_layer(world, layers[1], scroll, time);
        draw_layer(world, layers[2], scroll, time);

        // The street, its edge lit by the city.
        world.rect({0.f, road_y, width, height - road_y}, tgx::Color::rgb(0x0d0c16));
        world.rect({0.f, road_y, width, 1.f}, tgx::Color::rgb(0x2a2840));
        for (float x = -std::fmod(scroll, 24.f); x < width; x += 24.f) {
            world.rect({x, road_y + 11.f, 10.f, 1.f}, tgx::Color::rgb(0x3a3850));
        }
        const float lamp_offset = -std::fmod(scroll, 80.f);
        for (float x = lamp_offset; x < width + 80.f; x += 80.f) {
            draw_lamp(world, light, std::round(x + 40.f));
        }
        draw_car(world, light, std::round(car_x));

        for (const Drop &drop: drops) {
            world.line(drop.position, drop.position - rain_direction * 4.f, tgx::Color::rgb(0x8fa3d9).fade(0.22f));
        }
        for (const Splash &splash: splashes) {
            world.rect({std::round(splash.position.x), std::round(splash.position.y), 1.f, 1.f},
                       tgx::Color::rgb(0xb7c6f0).fade(splash.life / splash_life));
        }

        // The lantern: a warm glow, brighter towards the middle.
        const tgx::Color lantern = tgx::Color::rgb(0xffb35c);
        light.circle_gradient(mouse, 28.f, lantern.fade(0.28f), lantern.fade(0.f));
        if (flash > 0.f) {
            light.rect({0.f, 0.f, width, height}, strike_glow.fade(0.25f * flash));
        }

        // The target onto the window, through the shader; the words on top of
        // it, sharp, at the window's own resolution.
        screen.clear(tgx::colors::black);
        tube.sprite(pixels->texture(), {.position = position, .size = size});
        screen.text(position + tgx::Vec2{16.f, 40.f}, "NIGHT CITY", tgx::colors::white.fade(0.85f), 32.f);
        screen.text(
            position + tgx::Vec2{16.f, size.y - 32.f},
            "mouse: lantern   space: lightning   f12: screenshot",
            tgx::colors::light_gray.fade(0.7f)
        );

        screen.fps({10, 10});

        // The frame as it is now, before it goes to the display.
        if (app->input().pressed(tgx::Key::f12) && !app->window().minimized()) {
            const auto saved = app->device().read().save("night_city.png");
            std::println("screenshot: {}", saved ? "night_city.png" : tgx::to_str(saved.error()));
        }

        app->swap_buffers();
    }

    return 0;
}
