// Fireworks over a lake at night. Rockets rise from behind the city and
// burst as spheres, tilted double rings, drooping gold willows and crackling
// strobes; each burst lights up the sky for a moment. The lake below mirrors
// it all, dimmer and wavering with the ripples.
//
// Click: a rocket to that point (on the lake: to its reflection).
// Space: a finale volley.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <print>
#include <vector>

namespace {
    constexpr float pi = std::numbers::pi_v<float>;
    // The shore line, as a share of the window height.
    constexpr float horizon_share = 0.62f;
    constexpr std::size_t max_particles = 20000;
    constexpr int glow_size = 64;
    constexpr float flash_life = 0.7f;
    // Of the city's windows.
    constexpr float lit_share = 0.14f;

    // Seeded: the same show on every run.
    tgx::Random rng{20261015};

    constexpr std::array palette{
        tgx::Color::rgb(0xffc65c), tgx::Color::rgb(0xff4f7b), tgx::Color::rgb(0x5ad8ff),
        tgx::Color::rgb(0xb08cff), tgx::Color::rgb(0x8dff7a), tgx::Color::rgb(0xff7a3a),
        tgx::Color::rgb(0xe6eeff), tgx::Color::rgb(0xff3b3b),
    };
    constexpr tgx::Color silver = palette[6];
    constexpr auto gold = tgx::Color::rgb(0xffb347);
    // Top, middle, horizon: purple low down, from the city's lights.
    constexpr std::array sky{tgx::Color::rgb(0x03040b), tgx::Color::rgb(0x0b0d2a), tgx::Color::rgb(0x2a1f4d)};
    constexpr auto hills = tgx::Color::rgb(0x12122b);
    constexpr auto city = tgx::Color::rgb(0x06060e);
    constexpr auto water_top = tgx::Color::rgb(0x0e0f28);
    constexpr auto water_bottom = tgx::Color::rgb(0x020309);
    constexpr auto lamp = tgx::Color::rgb(0xffcf7a);

    // A flash is the sky lit up around a burst: one big, faint glow.
    enum class Kind : std::uint8_t { spark, willow, crackle, ember, flash };
    enum class Style : std::uint8_t { sphere, ring, willow, crackle };

    struct Particle {
        tgx::Vec2 position{}, velocity{};
        tgx::Color color{};
        float age{}, life{}, size{}, drag{}, gravity{};
        Kind kind{};
        float seed{};
    };

    struct Rocket {
        tgx::Vec2 from{}, to{};
        float delay{};
        tgx::Timer flight;
        Style style{};
    };

    // White, its alpha falling off from a bright core: one sprite per spark.
    [[nodiscard]] auto make_glow() -> tgx::Image {
        constexpr float half = glow_size * 0.5f;
        auto image = tgx::Image::create({glow_size, glow_size});
        for (int y = 0; y < glow_size; ++y) {
            for (int x = 0; x < glow_size; ++x) {
                const tgx::Vec2 d{static_cast<float>(x) + 0.5f - half, static_cast<float>(y) + 0.5f - half};
                const float r = tgx::length(d) / half;
                const float halo = std::pow(std::max(1.f - r, 0.f), 2.f) * 0.5f;
                image[x, y] = tgx::colors::white.fade(std::min(halo + std::exp(-r * r * 14.f), 1.f));
            }
        }
        return image;
    }

    // Reserved up front, so a full vector skips instead of allocating.
    auto emit(std::vector<Particle> &particles, const Particle &particle) -> void {
        if (particles.size() < particles.capacity()) {
            particles.push_back(particle);
        }
    }

    // height scales speeds and sizes with the window.
    auto burst(std::vector<Particle> &particles, tgx::Vec2 at, Style style, float height) -> void {
        const tgx::Color main = rng.pick(palette);
        const tgx::Color second = rng.pick(palette);
        const float speed = rng.next_float(0.3f, 0.38f) * height;
        const tgx::Color sky_light = style == Style::willow ? gold : main;
        emit(particles, {at, {}, sky_light, 0.f, flash_life, 1.1f * height, 0.f, 0.f, Kind::flash, 0.f});
        // Willows are heavier, so they droop.
        const auto spark = [&](tgx::Vec2 velocity, tgx::Color color, float life, Kind kind) {
            const bool willow = kind == Kind::willow;
            emit(particles, {at, velocity, color, 0.f, life, 0.016f * height, willow ? 1.8f : 1.5f,
                             (willow ? 0.2f : 0.09f) * height, kind, rng.next_float(0.f, 2.f * pi)});
        };
        // A point of a sphere seen from the side: denser in the middle.
        const auto shell = [] {
            const float z = rng.next_float(-1.f, 1.f);
            return rng.direction() * std::sqrt(1.f - z * z);
        };
        switch (style) {
        case Style::sphere:
            for (int i = 0; i < 260; ++i) {
                // An inner shell of the second color.
                const bool inner = i < 70;
                spark(shell() * speed * (inner ? 0.5f : rng.next_float(0.9f, 1.f)), inner ? second : main,
                      rng.next_float(1.5f, 2.1f), Kind::spark);
            }
            break;
        case Style::ring: {
            // Two circles seen at an angle, tilted.
            const tgx::Vec2 across = tgx::from_angle(rng.next_float(-0.6f, 0.6f));
            const tgx::Vec2 up = -tgx::perpendicular(across) * rng.next_float(0.35f, 0.9f);
            for (int i = 0; i < 120; ++i) {
                // Sparks in pairs, one on each circle.
                const int pair = i / 2;
                const tgx::Vec2 flat = tgx::from_angle(2.f * pi * static_cast<float>(pair) / 60.f);
                const bool outer = i % 2 == 0;
                spark((flat.x * across + flat.y * up) * speed * (outer ? 1.f : 0.6f), outer ? main : second,
                      rng.next_float(1.6f, 1.9f), Kind::spark);
            }
            break;
        }
        case Style::willow:
            for (int i = 0; i < 90; ++i) {
                spark(shell() * speed * 0.9f, gold, rng.next_float(2.8f, 3.6f), Kind::willow);
            }
            break;
        case Style::crackle:
            for (int i = 0; i < 180; ++i) {
                spark(shell() * speed * 0.85f, i % 2 == 0 ? main : silver, rng.next_float(1.1f, 1.6f), Kind::crackle);
            }
            break;
        }
    }

    // White-hot at first, then its color, cooling and fading out.
    [[nodiscard]] auto tint_of(const Particle &p) -> tgx::Color {
        const float t = p.age / p.life;
        if (p.kind == Kind::flash) {
            return p.color.fade(0.16f * tgx::ease::in_quad(1.f - t));
        }
        // A quick fade in, so a fresh burst is not one white blob.
        float fade = (1.f - tgx::ease::in_cubic(t)) * tgx::ease::out_quad(p.age / 0.12f);
        if (p.kind == Kind::ember) {
            fade = 0.5f * (1.f - tgx::ease::in_out_sine(t));
        } else if (p.kind == Kind::crackle && t > 0.4f && std::sin(p.age * 50.f + p.seed) < 0.f) {
            // The strobe: off half the time once it slows down.
            return tgx::colors::transparent;
        }
        const tgx::Color hot = tgx::lerp(tgx::colors::white, p.color, 0.4f + 0.6f * tgx::ease::out_quad(p.age / 0.2f));
        return tgx::lerp(hot, tgx::Color::rgb(0x601808), tgx::ease::in_quad(t) * 0.6f).fade(fade);
    }

    // A glow stretched along its motion: a streak when fast.
    auto draw_glow(
        tgx::Canvas &canvas, const tgx::Texture &glow, tgx::Vec2 at, tgx::Vec2 velocity, float size, tgx::Color tint
    ) -> void {
        const float length = std::max(size, tgx::length(velocity) * 0.09f);
        canvas.sprite(glow, {.position = at, .size = {length, size}, .origin = {length * 0.5f, size * 0.5f},
                             .rotation = tgx::angle(velocity), .tint = tint});
    }

    // How far the ripples shift a reflected point sideways.
    [[nodiscard]] auto waver(float y, float horizon, float time) -> float {
        return std::sin(y * 0.045f + time * 2.3f) * (1.5f + (y - horizon) * 0.03f);
    }

    // A random style by default.
    auto launch(std::vector<Rocket> &rockets, tgx::Vec2 to, float horizon, float delay = 0.f,
                Style style = static_cast<Style>(rng.next_int(0, 3))) -> void {
        if (rockets.size() < rockets.capacity()) {
            const tgx::Vec2 from{to.x + rng.next_float(-40.f, 40.f), horizon};
            rockets.push_back({from, to, delay, tgx::Timer::once(rng.next_float(1.f, 1.4f)), style});
        }
    }
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - scenes 03 fireworks"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto glow = tgx::Texture::create(make_glow(), {.mipmaps = true});
    if (!glow) {
        std::println(stderr, "glow: {}", glow.error());
        return 1;
    }

    std::vector<Particle> particles{};
    particles.reserve(max_particles);
    std::vector<Rocket> rockets{};
    rockets.reserve(128);
    auto next_launch = tgx::Timer::once(0.1f);

    auto &canvas = app->canvas();
    tgx::Canvas light = canvas;
    light.set_blend(tgx::Blend::additive);

    while (!app->should_close()) {
        app->poll_events();
        // Minimized, on Windows: no size to lay anything out in.
        if (canvas.size().empty()) {
            app->swap_buffers();
            continue;
        }

        const auto &input = app->input();
        const float dt = std::min(app->clock().delta(), 0.05f);
        const auto time = static_cast<float>(app->clock().elapsed());
        const auto [width, height] = tgx::to_vec2(canvas.size());
        const float horizon = std::round(height * horizon_share);
        // The whole sky fits in the lake's reflection.
        const float squeeze = (height - horizon) / horizon;
        const auto mirror = [&](float y) { return horizon + (horizon - y) * squeeze; };

        // Launches: on their own, at a click, and the finale.
        if (next_launch.tick(dt)) {
            launch(rockets, {rng.next_float(0.12f, 0.88f) * width, rng.next_float(0.12f, 0.4f) * height}, horizon);
            next_launch = tgx::Timer::once(rng.next_float(0.4f, 1.5f));
        }
        if (input.pressed(tgx::MouseButton::left)) {
            const tgx::Vec2 to = input.mouse();
            const float y = to.y > horizon ? horizon - (to.y - horizon) / squeeze : to.y;
            launch(rockets, {to.x, std::min(y, horizon - 0.12f * height)}, horizon);
        }
        if (input.pressed(tgx::Key::space)) {
            for (int i = 0; i < 16; ++i) {
                const float x = (0.1f + 0.8f * static_cast<float>(i) / 15.f) * width;
                // Two waves of 8.
                const int wave = i / 8;
                const float delay = static_cast<float>(i % 8) * 0.18f + static_cast<float>(wave) * 0.9f;
                launch(rockets, {x, rng.next_float(0.12f, 0.35f) * height}, horizon, delay);
            }
            // Ends with a wall of willows.
            for (int i = 0; i < 4; ++i) {
                const float x = (0.2f + 0.2f * static_cast<float>(i)) * width;
                launch(rockets, {x, 0.16f * height}, horizon, 2.6f, Style::willow);
            }
        }

        // The sky with its stars, and the lake below.
        canvas.clear(water_bottom);
        canvas.rect_gradient({0.f, 0.f, width, horizon * 0.55f}, sky[0], sky[1]);
        canvas.rect_gradient({0.f, horizon * 0.55f, width, horizon * 0.45f}, sky[1], sky[2]);
        // The same stars every frame; fewer low down, in the city's glow.
        tgx::Random stars{11};
        for (int i = 0; i < 260; ++i) {
            const float y = std::pow(stars.next_float(), 1.6f) * 0.95f;
            const tgx::Vec2 at{stars.next_float(0.f, width), y * horizon};
            const float star = stars.next_float(1.f, 2.2f);
            const float twinkle = (0.45f + 0.35f * std::sin(time * 1.7f + stars.next_float(0.f, 20.f))) * (1.f - y);
            light.rect(tgx::Rect::from_center(at, {star, star}), tgx::Color::rgb(0xcfd8ff).fade(twinkle));
        }
        canvas.rect_gradient({0.f, horizon, width, height - horizon}, water_top, water_bottom);

        // Light adds up in any order: each thing is drawn as it moves.
        for (Rocket &r : rockets) {
            r.delay -= dt;
            if (r.delay > 0.f) {
                continue;
            }
            if (r.flight.tick(dt)) {
                burst(particles, r.to, r.style, height);
                continue;
            }
            const float t = r.flight.progress();
            const tgx::Vec2 at = tgx::lerp(r.from, r.to, tgx::ease::out_quad(t));
            draw_glow(light, *glow, at, (r.to - r.from) * (1.f - t), 0.012f * height, tgx::Color::rgb(0xfff0c8));
            // A trail of embers, falling away behind.
            const tgx::Vec2 drift = rng.point_in({-12.f, 10.f, 24.f, 30.f});
            emit(particles, {at, drift, gold, 0.f, rng.next_float(0.4f, 0.8f), 0.009f * height, 1.5f, 0.f,
                             Kind::ember, 0.f});
        }
        std::erase_if(rockets, [](const Rocket &r) { return r.flight.done(); });

        // By index: the loop may add particles, within the reserved size.
        for (std::size_t i = 0; i < particles.size(); ++i) {
            Particle &p = particles[i];
            p.age += dt;
            p.velocity *= std::exp(-p.drag * dt);
            p.velocity.y += p.gravity * dt;
            p.position += p.velocity * dt;
            // A willow leaves a long trail of embers that sink slowly.
            if (p.kind == Kind::willow && rng.chance(dt * 30.f)) {
                emit(particles, {p.position, p.velocity * 0.1f, p.color, 0.f, rng.next_float(1.f, 1.6f), p.size * 0.7f,
                                 1.5f, 0.02f * height, Kind::ember, 0.f});
            }
            const tgx::Color tint = tint_of(p);
            draw_glow(light, *glow, p.position, p.velocity, p.size, tint);
            // Mirrored in the lake, squeezed to fit, dimmer further out.
            const float y = mirror(p.position.y);
            const float dim = 0.45f - 0.27f * (y - horizon) / (height - horizon);
            const tgx::Vec2 at{p.position.x + waver(y, horizon, time), y};
            draw_glow(light, *glow, at, {p.velocity.x, -p.velocity.y * squeeze}, p.size * 1.2f,
                      tint.fade(static_cast<float>(tint.a) / 255.f * dim));
        }
        std::erase_if(particles, [&](const Particle &p) { return p.age >= p.life || p.position.y > horizon + 4.f; });

        // Far hills, then the city with its lit windows, over the sparks that
        // fell behind them, each with its reflection.
        constexpr float step = 2.f;
        for (float x = 0.f; x < width; x += step) {
            const float u = x / width;
            const float rise = height * (0.07f + 0.03f * std::sin(u * 6.3f + 1.f) + 0.015f * std::sin(u * 17.f));
            canvas.rect({x, horizon - rise, step, rise}, hills);
            canvas.rect({x, horizon, step, rise * squeeze}, hills.fade(0.7f));
        }
        // The same city every frame, in shares of the window.
        tgx::Random blocks{3};
        const float cell = std::max(std::round(0.007f * height), 3.f);
        for (float share = 0.f; share < 1.f;) {
            const float wide = blocks.next_float(0.012f, 0.035f);
            const float tall = std::round(blocks.next_float(0.02f, 0.085f) * height);
            const tgx::Rect rect{std::round(share * width), horizon - tall, std::round(wide * width), tall};
            share += wide + blocks.next_float(0.f, 0.004f);
            canvas.rect(rect, city);
            canvas.rect({rect.x, horizon, rect.width, tall * squeeze}, city.fade(0.75f));
            // Its own seed, so the windows do not shift the next buildings.
            tgx::Random windows{blocks.next_u32()};
            for (float y = rect.y + cell; y < horizon - cell; y += cell * 1.6f) {
                for (float x = rect.x + cell * 0.6f; x + cell < rect.right(); x += cell) {
                    if (windows.chance(lit_share)) {
                        const float alpha = windows.next_float(0.35f, 0.8f);
                        const float my = mirror(y + cell * 0.7f);
                        canvas.rect({x, y, cell * 0.5f, cell * 0.7f}, lamp.fade(alpha));
                        light.rect({x + waver(my, horizon, time), my, cell * 0.5f, cell}, lamp.fade(alpha * 0.3f));
                    }
                }
            }
        }

        canvas.text({16.f, height - 44.f}, "FIREWORKS OVER THE LAKE", tgx::colors::white.fade(0.7f));
        canvas.text({16.f, height - 24.f}, "click: a rocket there   space: finale", tgx::colors::white.fade(0.45f));

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
