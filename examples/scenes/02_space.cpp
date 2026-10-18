// Deep space. Three layers of stars drift at their own speeds over a soft
// nebula; a ringed gas giant and its moon hang in the light of a sun off the
// top-left corner; rocks tumble past. The planet, rings and moon are lit in
// 3D: the rings pass behind the planet and in front of it, and its shadow
// falls on them.
//
// The mouse steers a small ship. Click: a shockwave.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numbers>
#include <print>
#include <utility>
#include <vector>

namespace {
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float wave_time = 1.4f;

    // Seeded: the same sky on every run.
    tgx::Random rng{20261015};

    struct Star { tgx::Vec2 uv; float depth, size, phase, rate; tgx::Color tint; };
    struct Blob { tgx::Vec2 uv; float radius, phase; tgx::Color color; };
    // x in pixels, y a fraction of the height: a resize keeps the belt low.
    struct Rock { tgx::Vec2 pos; float radius, angle, spin, depth; tgx::Color color; std::array<float, 9> shape; };
    // A glowing dot, the ship's trail or a shockwave's burst, slowing down.
    struct Spark { tgx::Vec2 from, vel; float born, life, size; tgx::Color hot, cool; };
    struct Wave { tgx::Vec2 pos; float born; };
    // One ring of the planet, in planet radii.
    struct Band { float inner, outer; tgx::Color color; float alpha; };

    constexpr std::array bands{
        Band{1.32f, 1.48f, tgx::Color::rgb(0x6e5e58), 0.35f},
        Band{1.48f, 1.6f, tgx::Color::rgb(0xb09878), 0.7f},
        Band{1.6f, 1.74f, tgx::Color::rgb(0xc9b090), 0.8f},
        Band{1.74f, 1.84f, tgx::Color::rgb(0xe8d3b0), 0.9f},
        Band{1.84f, 1.93f, tgx::Color::rgb(0xd5bd98), 0.85f},
        Band{2.f, 2.1f, tgx::Color::rgb(0xb09a82), 0.7f},
        Band{2.1f, 2.22f, tgx::Color::rgb(0x9a846e), 0.6f},
        Band{2.27f, 2.33f, tgx::Color::rgb(0x8c7c70), 0.45f},
    };

    auto smooth(float from, float to, float x) -> float {
        const float t = std::clamp((x - from) / (to - from), 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    auto wrap(float value, float span) -> float {
        const float v = std::fmod(value, span);
        return v < 0.f ? v + span : v;
    }

    // Darker by k in [0, 1].
    auto shade(tgx::Color color, float k) -> tgx::Color { return tgx::lerp(tgx::colors::black, color, k); }

    // A lit ball as a disc of rings, each corner colored by surface(normal),
    // z towards the back. The rings crowd towards the rim, where the light
    // changes fastest.
    template<std::size_t sides, typename Surface>
    auto sphere(tgx::Canvas &canvas, tgx::Vec2 center, float radius, int rings, Surface surface) -> void {
        // The middle starts as a ring of one point.
        std::array<tgx::Vec2, sides> pos{}, last_pos{};
        std::array<tgx::Color, sides> color{}, last_color{};
        last_pos.fill(center);
        last_color.fill(surface(tgx::Vec3{0.f, 0.f, -1.f}));
        for (int k = 1; k <= rings; ++k) {
            const float rho = tgx::ease::out_sine(static_cast<float>(k) / static_cast<float>(rings));
            for (std::size_t i = 0; i < sides; ++i) {
                const tgx::Vec2 dir = tgx::from_angle(2.f * pi * static_cast<float>(i) / static_cast<float>(sides));
                pos[i] = center + dir * (rho * radius);
                color[i] = surface(tgx::Vec3{dir.x * rho, dir.y * rho, -std::sqrt(std::max(1.f - rho * rho, 0.f))});
            }
            for (std::size_t i = 0; i < sides; ++i) {
                const std::size_t j = (i + 1) % sides;
                canvas.triangle_gradient(last_pos[i], pos[i], pos[j], last_color[i], color[i], color[j]);
                canvas.triangle_gradient(last_pos[i], pos[j], last_pos[j], last_color[i], color[j], last_color[j]);
            }
            last_pos = pos;
            last_color = color;
        }
    }
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - scenes 02 space"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &canvas = app->canvas();

    // Light from the sun off the top-left corner, a little towards us.
    const tgx::Vec3 sun = tgx::normalize(tgx::Vec3{-0.62f, -0.5f, -0.6f});
    const tgx::Vec2 sun_2d = tgx::normalize(tgx::Vec2{sun.x, sun.y});
    // The planet's axes, tilted towards us and rolled: rings along e1 and e3.
    constexpr float tilt = 0.32f, roll = -0.3f;
    const float ct = std::cos(tilt), st = std::sin(tilt), cr = std::cos(roll), sr = std::sin(roll);
    const tgx::Vec3 e1{cr, sr, 0.f}, e2{-ct * sr, ct * cr, st}, e3{st * sr, -st * cr, ct};

    // Stars in layers, the far ones many, small, dim and slow; white, a little tinted.
    constexpr std::array layers{std::pair{260, 0.15f}, std::pair{120, 0.4f}, std::pair{40, 0.8f}};
    std::vector<Star> stars{};
    for (const auto &[count, depth] : layers) {
        for (int i = 0; i < count; ++i) {
            const tgx::Vec2 uv = rng.point_in({0.f, 0.f, 1.f, 1.f});
            const float size = depth * rng.next_float(1.2f, 2.4f) + rng.next_float(0.3f, 0.6f);
            const tgx::Color tint = tgx::Color::hsv(rng.next_float(20.f, 240.f), rng.next_float(0.f, 0.25f), 1.f);
            stars.push_back({uv, depth, size, rng.next_float(0.f, 2.f * pi), rng.next_float(0.6f, 2.5f), tint});
        }
    }

    // The nebula: soft blobs, teal to violet to pink, on a band from the bottom-left up.
    constexpr std::array hues{285.f, 215.f, 185.f, 205.f, 330.f};
    std::vector<Blob> nebula{};
    for (int i = 0; i < 22; ++i) {
        const tgx::Vec2 along = tgx::lerp(tgx::Vec2{0.05f, 0.95f}, tgx::Vec2{0.75f, -0.05f}, rng.next_float(0.f, 1.f));
        const tgx::Vec2 uv = along + tgx::Vec2{0.7f, 0.5f} * rng.next_float(-0.18f, 0.18f);
        const tgx::Color hue = tgx::Color::hsv(hues[static_cast<std::size_t>(i) % hues.size()], 0.75f, 0.62f);
        nebula.push_back({uv, rng.next_float(0.18f, 0.36f), rng.next_float(0.f, 2.f * pi), hue});
    }

    // Rocks from far (small, dark, slow) to near, so the near ones draw last.
    constexpr std::array stones{tgx::Color::rgb(0x9a8270), tgx::Color::rgb(0x86808e)};
    const tgx::Vec2 start{static_cast<float>(canvas.size().width), static_cast<float>(canvas.size().height)};
    std::vector<Rock> rocks{};
    for (int i = 1; i <= 18; ++i) {
        const float depth = static_cast<float>(i) / 18.f;
        const tgx::Vec2 pos{rng.next_float(0.f, 1.f) * start.x, rng.next_float(0.55f, 0.86f)};
        const tgx::Color stone = tgx::lerp(tgx::Color::rgb(0x3a3d55), rng.pick(stones), 0.3f + 0.7f * depth);
        const float spin = rng.next_float(-0.6f, 0.6f);
        Rock rock{pos, 5.f + 24.f * depth * depth, rng.next_float(0.f, 2.f * pi), spin, depth, stone, {}};
        std::ranges::generate(rock.shape, [] { return rng.next_float(0.7f, 1.05f); });
        rocks.push_back(rock);
    }

    std::vector<Spark> sparks{};
    sparks.reserve(2048);
    std::vector<Wave> waves{};
    waves.reserve(16);

    tgx::Vec2 ship = start * 0.4f, ship_vel{};
    float heading = 0.f, exhaust = 0.f;

    while (!app->should_close()) {
        app->poll_events();

        const float dt = std::min(app->clock().delta(), 0.05f);
        const auto time = static_cast<float>(app->clock().elapsed());
        const float w = static_cast<float>(canvas.size().width), h = static_cast<float>(canvas.size().height);

        // The ship chases the mouse at a capped speed, or idles in a loop
        // while the mouse is away, and turns smoothly to where it goes.
        const tgx::Vec2 mouse = app->input().mouse();
        const bool has_mouse = mouse.x > 0.f && mouse.y > 0.f && mouse.x < w && mouse.y < h;
        const tgx::Vec2 idle{w * (0.3f + 0.12f * std::cos(time * 0.4f)), h * (0.45f + 0.1f * std::sin(time * 0.8f))};
        tgx::Vec2 want = ((has_mouse ? mouse : idle) - ship) * 2.5f;
        want = tgx::length(want) > 520.f ? tgx::normalize(want) * 520.f : want;
        ship_vel = tgx::lerp(ship_vel, want, 1.f - std::exp(-4.f * dt));
        ship += ship_vel * dt;
        const float thrust = std::min(tgx::length(ship_vel) / 520.f, 1.f);
        if (thrust > 0.03f) {
            heading += std::remainder(tgx::angle(ship_vel) - heading, 2.f * pi) * std::min(1.f, 7.f * dt);
        }
        const auto on_ship = [&](tgx::Vec2 local) { return ship + tgx::rotate(local * 1.4f, heading); };
        const tgx::Vec2 forward = tgx::from_angle(heading);

        for (exhaust += dt * (50.f + 220.f * thrust); exhaust >= 1.f; exhaust -= 1.f) {
            const tgx::Vec2 vel = forward * -rng.next_float(60.f, 160.f) + ship_vel * 0.2f + rng.direction() * 12.f;
            sparks.push_back({on_ship({-8.f, rng.next_float(-2.f, 2.f)}), vel, time, rng.next_float(0.4f, 0.8f),
                              rng.next_float(2.f, 3.5f), tgx::Color::rgb(0xffd58a), tgx::Color::rgb(0xb03cff)});
        }
        if (app->input().pressed(tgx::MouseButton::left)) {
            waves.push_back({ship, time});
            for (int i = 0; i < 60; ++i) {
                sparks.push_back({ship, rng.direction() * rng.next_float(80.f, 420.f), time, rng.next_float(0.5f, 1.1f),
                                  rng.next_float(1.5f, 3.f), tgx::Color::rgb(0xe8fbff), tgx::Color::rgb(0x3d7bff)});
            }
        }
        std::erase_if(waves, [&](const Wave &wave) { return time - wave.born > wave_time; });
        std::erase_if(sparks, [&](const Spark &spark) { return time - spark.born >= spark.life; });

        // Rocks drift left, the near ones faster, around the window's edges.
        for (Rock &rock : rocks) {
            const float m = rock.radius * 1.5f;
            rock.pos.x = wrap(rock.pos.x - (8.f + 40.f * rock.depth) * dt + m, w + 2.f * m) - m;
            rock.angle += rock.spin * dt;
        }

        // Sky: a dark gradient, the sun's glare, the nebula, the stars. The
        // ship's offset from the middle leans the far layers a little.
        canvas.set_blend(tgx::Blend::alpha);
        canvas.rect_gradient({0.f, 0.f, w, h}, tgx::Color::rgb(0x0b0d22), tgx::Color::rgb(0x0a0615));
        canvas.set_blend(tgx::Blend::additive);
        const float diagonal = std::hypot(w, h);
        constexpr tgx::Color glare = tgx::Color::rgb(0xffcf9a);
        canvas.circle_gradient({-0.05f * w, -0.1f * h}, 0.75f * diagonal, glare.fade(0.22f), glare.fade(0.f));
        const tgx::Vec2 lean = (ship - tgx::Vec2{w / 2.f, h / 2.f}) * -0.02f;
        for (const Blob &blob : nebula) {
            const tgx::Vec2 sway = tgx::from_angle(time * 0.05f + blob.phase) * 12.f;
            const tgx::Vec2 at = tgx::Vec2{blob.uv.x * w, blob.uv.y * h} + lean + sway;
            const float glow = 0.1f + 0.03f * std::sin(time * 0.3f + blob.phase);
            canvas.circle_gradient(at, blob.radius * diagonal, blob.color.fade(glow), blob.color.fade(0.f));
        }
        const tgx::Vec2 pan = tgx::Vec2{-14.f, -3.f} * time + lean * 4.f;
        for (const Star &star : stars) {
            const tgx::Vec2 shift = pan * star.depth;
            const tgx::Vec2 at{wrap(star.uv.x * w + shift.x, w), wrap(star.uv.y * h + shift.y, h)};
            const float shine = 0.65f + 0.35f * std::sin(time * star.rate + star.phase);
            canvas.circle(at, star.size * 0.5f, star.tint.fade(shine * (0.4f + 0.6f * star.depth)));
            if (star.size > 1.6f) {
                canvas.circle_gradient(at, star.size * 4.f, star.tint.fade(0.16f * shine), star.tint.fade(0.f));
            }
        }

        // The planet system, in planet radii, z away from us. The far half of
        // the rings goes before the planet, the near half after; the parts in
        // its shadow (behind it from the sun, within its radius) dim.
        const float radius = 0.21f * std::min(w, h);
        const tgx::Vec2 center{w * 0.64f, h * 0.48f};
        const auto on_screen = [&](tgx::Vec3 p) { return center + tgx::Vec2{p.x, p.y} * radius; };
        const auto at_ring = [&](float r, float a) { return (e1 * std::cos(a) + e3 * std::sin(a)) * r; };
        const float ring_light = 0.55f + 0.45f * std::abs(tgx::dot(e2, sun));
        const auto draw_rings = [&](bool far) {
            constexpr int sides = 160;
            canvas.set_blend(tgx::Blend::alpha);
            for (const auto &[inner, outer, band_color, alpha] : bands) {
                for (int i = far ? 0 : sides / 2; i < (far ? sides / 2 : sides); ++i) {
                    const float a0 = 2.f * pi * static_cast<float>(i) / sides;
                    const float a1 = 2.f * pi * static_cast<float>(i + 1) / sides;
                    const tgx::Vec3 mid = at_ring((inner + outer) / 2.f, (a0 + a1) / 2.f);
                    const float toward = tgx::dot(mid, sun);
                    const float off = tgx::length(mid - sun * toward);
                    const float lit = toward > 0.f ? 1.f : 0.15f + 0.85f * smooth(0.9f, 1.05f, off);
                    const tgx::Color color = shade(band_color, ring_light * lit).fade(alpha);
                    const tgx::Vec2 p0 = on_screen(at_ring(inner, a0)), p2 = on_screen(at_ring(outer, a1));
                    canvas.triangle(p0, on_screen(at_ring(outer, a0)), p2, color);
                    canvas.triangle(p0, p2, on_screen(at_ring(inner, a1)), color);
                }
            }
        };
        const float orbit = time * 0.16f;
        const tgx::Vec3 moon = at_ring(2.75f, orbit) + e2 * (0.25f * std::sin(orbit));
        const auto draw_moon = [&](bool far) {
            if (far == (moon.z > 0.f)) {
                sphere<48>(canvas, on_screen(moon), radius * 0.12f, 6, [&](tgx::Vec3 n) {
                    return shade(tgx::Color::rgb(0xb4b6c8), 0.1f + 0.9f * smooth(-0.15f, 0.7f, tgx::dot(n, sun)));
                });
            }
        };
        draw_moon(true);
        draw_rings(true);
        canvas.set_blend(tgx::Blend::additive);
        constexpr tgx::Color haze = tgx::Color::rgb(0xffb070);
        canvas.circle_gradient(center + sun_2d * (radius * 0.08f), radius * 1.3f, haze.fade(0.3f), haze.fade(0.f));
        // Bands by latitude, the poles duller; the night side keeps a little blue.
        canvas.set_blend(tgx::Blend::alpha);
        sphere<112>(canvas, center, radius, 16, [&](tgx::Vec3 n) {
            const float lat = tgx::dot(n, e2);
            const float stripes = 0.5f + 0.5f * std::sin(lat * 13.f + std::sin(lat * 31.f) * 0.9f);
            const tgx::Color band = tgx::lerp(tgx::Color::rgb(0xf0d2a8), tgx::Color::rgb(0xc58658), stripes);
            const tgx::Color color = tgx::lerp(band, tgx::Color::rgb(0x7d6a78), smooth(0.6f, 0.95f, std::abs(lat)));
            const float light = smooth(-0.12f, 0.75f, tgx::dot(n, sun)) * (0.6f + 0.4f * std::sqrt(-n.z));
            return tgx::lerp(tgx::Color::rgb(0x0b0c1c), color, light);
        });
        // Rims: warm on the sunny side (the wider arcs fainter), cool on the night side.
        canvas.set_blend(tgx::Blend::additive);
        const float to_sun = tgx::angle(sun_2d);
        for (int i = 0; i < 3; ++i) {
            const float span = 1.7f - 0.5f * static_cast<float>(i);
            const tgx::Color warm = tgx::Color::rgb(0xffe2b8).fade(0.14f), cool = tgx::Color::rgb(0x6f8cff).fade(0.08f);
            canvas.ring(center, radius * 0.975f, radius * 1.003f, to_sun - span, to_sun + span, warm);
            const float night = to_sun + pi;
            canvas.ring(center, radius * 0.985f, radius * 1.003f, night - span * 0.6f, night + span * 0.6f, cool);
        }
        draw_rings(false);
        draw_moon(false);

        // Rocks: each a fan lit on the sun's side, with a dark edge.
        for (const Rock &rock : rocks) {
            const tgx::Vec2 at{rock.pos.x, rock.pos.y * h};
            std::array<tgx::Vec2, 9> corners{};
            std::array<tgx::Color, 9> shades{};
            for (std::size_t i = 0; i < corners.size(); ++i) {
                const tgx::Vec2 dir = tgx::from_angle(rock.angle + 2.f * pi * static_cast<float>(i) / 9.f);
                corners[i] = at + dir * (rock.radius * rock.shape[i]);
                shades[i] = shade(rock.color, 0.35f + 0.65f * std::max(tgx::dot(dir, sun_2d), 0.f));
            }
            for (std::size_t i = 0; i < corners.size(); ++i) {
                const std::size_t j = (i + 1) % corners.size();
                canvas.triangle_gradient(at, corners[i], corners[j], shade(rock.color, 0.6f), shades[i], shades[j]);
            }
            canvas.line_loop(corners, shade(rock.color, 0.4f), 1.f);
        }

        // Light: sparks, shockwaves, the engine's flame.
        canvas.set_blend(tgx::Blend::additive);
        constexpr tgx::Color ice = tgx::Color::rgb(0x6cc4ff), white = tgx::colors::white;
        for (const Spark &spark : sparks) {
            const float age = time - spark.born, t = age / spark.life;
            const tgx::Color color = tgx::lerp(spark.hot, spark.cool, t).fade((1.f - t) * (1.f - t) * 0.8f);
            const tgx::Vec2 at = spark.from + spark.vel * ((1.f - std::exp(-1.5f * age)) / 1.5f);
            canvas.circle_gradient(at, spark.size * (1.f + t), color, color.fade(0.f));
        }
        for (const Wave &wave : waves) {
            const float t = (time - wave.born) / wave_time;
            const float r = tgx::ease::out_cubic(t) * 0.4f * std::min(w, h), fade = (1.f - t) * (1.f - t);
            canvas.ring(wave.pos, std::max(r - 4.f - 10.f * t, 0.f), r, 0.f, 2.f * pi, ice.fade(0.8f * fade));
            canvas.circle_gradient(wave.pos, 60.f * (1.f + t), white.fade(0.7f - 2.f * t), white.fade(0.f));
        }
        const float flame = (10.f + 22.f * thrust) * (0.85f + 0.15f * std::sin(time * 40.f) * std::sin(time * 23.f));
        constexpr tgx::Color fire = tgx::Color::rgb(0xffcf7a), ember = tgx::Color::rgb(0xff9d4a);
        canvas.circle_gradient(on_ship({-7.f, 0.f}), 20.f + 14.f * thrust, ember.fade(0.45f), ember.fade(0.f));
        const tgx::Vec2 tip = on_ship({-6.f - flame, 0.f}), core = on_ship({-6.f - flame / 2.f, 0.f});
        canvas.triangle_gradient(on_ship({-6.f, -4.f}), on_ship({-6.f, 4.f}), tip, fire, fire, ember.fade(0.f));
        canvas.triangle_gradient(on_ship({-6.f, -2.f}), on_ship({-6.f, 2.f}), core, white, white, fire.fade(0.f));

        // The ship: two halves, the sunny one lighter.
        canvas.set_blend(tgx::Blend::alpha);
        const bool left_lit = tgx::dot(tgx::perpendicular(forward), sun_2d) > 0.f;
        const tgx::Color hull = tgx::Color::rgb(0xe9ecf5), left = shade(hull, left_lit ? 1.f : 0.55f);
        const tgx::Color right = shade(hull, left_lit ? 0.55f : 1.f);
        const tgx::Vec2 nose = on_ship({16.f, 0.f}), notch = on_ship({-5.f, 0.f});
        canvas.triangle_gradient(nose, on_ship({-9.f, -11.f}), notch, hull, left, left);
        canvas.triangle_gradient(nose, notch, on_ship({-9.f, 11.f}), hull, right, right);
        canvas.circle(on_ship({4.f, 0.f}), 3.f, tgx::Color::rgb(0x5fe0ff));

        // A soft vignette at the top and bottom, then the title over it.
        canvas.rect_gradient({0.f, 0.f, w, h * 0.2f}, tgx::colors::black.fade(0.45f), tgx::colors::transparent);
        canvas.rect_gradient({0.f, h * 0.8f, w, h * 0.2f}, tgx::colors::transparent, tgx::colors::black.fade(0.5f));
        canvas.text({24.f, h - 44.f}, "DEEP SPACE", tgx::Color::rgb(0xe8e4ff), 32.f, {0.f, 1.f});
        canvas.text({26.f, h - 20.f}, "mouse: steer    click: shockwave", tgx::Color::rgb(0x8f97c4), 16.f, {0.f, 1.f});

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
