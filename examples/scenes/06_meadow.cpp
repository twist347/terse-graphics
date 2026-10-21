// A meadow through a whole day, in flat vector shapes. Rolling hills fade
// into the haze, the sky turns from dawn to noon to sunset to night, the sun
// and the moon cross on their arcs and clouds drift by. Grass and flowers
// sway in the wind; butterflies come out by day, stars and fireflies by night.
//
// Mouse wheel or Left/Right: time faster, slower or backwards. Space: pause
// time. The mouse parts the grass and draws the fireflies near.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <numbers>
#include <print>
#include <string_view>

namespace {
    using tgx::Color;
    using tgx::Vec2;

    constexpr float pi = std::numbers::pi_v<float>;
    constexpr Color clear = tgx::colors::transparent;

    // The light of a moment of the day, 0xRRGGBB: sky top and bottom, light on
    // the land (multiplies its colors), haze far things fade into, clouds.
    struct Palette { float hour; std::array<std::uint32_t, 5> colors; };
    struct Sky { Color top, bottom, light, haze, cloud; };
    // Night from 21 to 5; frames in between blend the two around them.
    constexpr std::array palettes{
        Palette{5.f, {0x0a0f2c, 0x26336a, 0x56669e, 0x23305e, 0x323d6c}},
        Palette{6.4f, {0x3a4888, 0xf2a47e, 0xb49ab8, 0xd0a2b0, 0xf6bca8}},
        Palette{8.f, {0x5ca4e4, 0xd6eef8, 0xf6f2e6, 0xb6d6ec, 0xffffff}},
        Palette{12.f, {0x3c8ce0, 0xc2e6f8, 0xffffff, 0xaad2f0, 0xffffff}},
        Palette{16.f, {0x4a8ed8, 0xdeeaee, 0xfff4e0, 0xbed6e6, 0xfffaf0}},
        Palette{17.4f, {0x5a84cc, 0xffdaa0, 0xffe2b4, 0xeed2b6, 0xfff0d8}},
        Palette{18.4f, {0x4a4c9e, 0xff9a5c, 0xf2a888, 0xe89a84, 0xffb48c}},
        Palette{19.7f, {0x1e245c, 0x8e5a8c, 0x76689e, 0x524a86, 0x70588c}},
        Palette{21.f, {0x0a0f2c, 0x26336a, 0x56669e, 0x23305e, 0x323d6c}},
    };

    // Hills, far to near (the meadow): where they lie (a fraction of the
    // height), how much they roll, their color by day, how much haze.
    constexpr std::array hill_base{0.57f, 0.64f, 0.72f, 0.81f};
    constexpr std::array hill_roll{0.065f, 0.055f, 0.04f, 0.025f};
    constexpr std::array hill_color{Color::rgb(0x7fb89a), Color::rgb(0x62a86a), Color::rgb(0x4f9a4a), Color::rgb(0x6cb246)};
    constexpr std::array hill_haze{0.62f, 0.4f, 0.18f, 0.f};
    struct Tree { std::size_t layer; float u; float size; bool pine; };
    constexpr std::array trees{
        Tree{1, 0.1f, 0.8f, true}, Tree{1, 0.13f, 0.6f, true}, Tree{1, 0.155f, 0.7f, true}, Tree{1, 0.6f, 0.65f, true},
        Tree{1, 0.63f, 0.85f, true}, Tree{2, 0.33f, 0.9f, false}, Tree{2, 0.365f, 0.7f, false},
        Tree{2, 0.93f, 0.8f, false}, Tree{3, 0.8f, 2.f, false},
    };

    // A cloud: a flat base and puffs on it, {x, y, radius} at scale 1.
    constexpr std::array puffs{tgx::Vec3{-30, -6, 20}, tgx::Vec3{-5, -17, 26}, tgx::Vec3{22, -9, 20}, tgx::Vec3{42, 1, 13}};
    constexpr std::array craters{tgx::Vec3{-7, -5, 5}, tgx::Vec3{7, 6, 4}, tgx::Vec3{8, -9, 2.5f}};
    constexpr std::array petals{Color::rgb(0xffffff), Color::rgb(0xffd34e), Color::rgb(0xff8fab), Color::rgb(0xc9a7ff)};
    constexpr std::array speeds{-16.f, -8.f, -4.f, -2.f, -1.f, 1.f, 2.f, 4.f, 8.f, 16.f};
    // Hours per second at 1x: a day in 160 seconds.
    constexpr float base_rate = 0.15f;
    struct Blade { float u, depth, length, lean, phase, bend; Color root, tip; };
    struct Firefly { Vec2 pos, vel; float phase; };

    // Channel by channel: light falling on a surface.
    auto mul(Color a, Color b) -> Color {
        const auto ch = [](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(x * y / 255); };
        return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), a.a};
    }

    auto sky_at(float clock) -> Sky {
        const float hour = std::clamp(clock, palettes.front().hour, palettes.back().hour);
        // The first palette after the hour, and the one before it.
        const auto next = std::min(std::ranges::upper_bound(palettes, hour, {}, &Palette::hour), palettes.end() - 1);
        const Palette &a = *(next - 1);
        const Palette &b = *next;
        const float t = tgx::ease::in_out_sine((hour - a.hour) / (b.hour - a.hour));
        const auto mix = [&](std::size_t c) { return lerp(Color::rgb(a.colors[c]), Color::rgb(b.colors[c]), t); };
        return {mix(0), mix(1), mix(2), mix(3), mix(4)};
    }

    auto hill_y(std::size_t layer, float x, Vec2 size) -> float {
        const auto l = static_cast<float>(layer);
        const float u = x / size.x * 2.f * pi;
        const float wave = 0.6f * std::sin(u * (1.1f + 0.5f * l) + l * 1.7f) + 0.4f * std::sin(u * (2.7f + 0.9f * l) + l * 4.1f);
        return size.y * (hill_base[layer] - hill_roll[layer] * wave);
    }

    // Hours since rising (0 to 14) to a point on the arc; eased a little.
    auto on_arc(float hours_up, Vec2 size) -> Vec2 {
        const float p = hours_up / 14.f;
        const float a = std::lerp(-0.22f, pi + 0.22f, std::lerp(p, tgx::ease::in_out_sine(p), 0.3f));
        return {size.x * (0.5f - 0.42f * std::cos(a)), size.y * (0.56f - 0.5f * std::sin(a))};
    }

    // A strip of quads down from the ridge, the top color fading to bottom.
    auto draw_hill(tgx::Canvas &canvas, std::size_t layer, Vec2 size, Color top, Color bottom) -> void {
        constexpr float step = 8.f;
        const float floor = size.y * (hill_base[layer] + hill_roll[layer] + 0.07f);
        for (float x = 0.f; x < size.x; x += step) {
            const Vec2 a{x, hill_y(layer, x, size)};
            const Vec2 b{x + step, hill_y(layer, x + step, size)};
            canvas.triangle_gradient(a, b, {b.x, floor}, top, top, bottom);
            canvas.triangle_gradient(a, {b.x, floor}, {a.x, floor}, top, bottom, bottom);
        }
        canvas.rect({0.f, floor, size.x, size.y - floor}, bottom);
    }

    auto draw_tree(tgx::Canvas &canvas, Vec2 at, float k, bool pine, Color leaves, Color trunk, Color lit) -> void {
        const auto p = [&](float x, float y) { return at + Vec2{x, y} * k; };
        if (pine) {
            canvas.rect({at.x - 2.f * k, at.y - 10.f * k, 4.f * k, 10.f * k}, trunk);
            canvas.triangle(p(-11.f, -7.f), p(0.f, -34.f), p(11.f, -7.f), leaves);
            canvas.triangle(p(-8.f, -22.f), p(0.f, -46.f), p(8.f, -22.f), leaves);
            canvas.triangle(p(0.f, -46.f), p(8.f, -22.f), p(2.f, -22.f), lit);
            return;
        }
        canvas.rect({at.x - 3.f * k, at.y - 24.f * k, 6.f * k, 25.f * k}, trunk);
        canvas.circle(p(-11.f, -25.f), 12.f * k, leaves);
        canvas.circle(p(11.f, -26.f), 12.f * k, leaves);
        canvas.circle(p(0.f, -36.f), 16.f * k, leaves);
        canvas.circle(p(-5.f, -41.f), 8.f * k, lit);
    }
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - scenes 06 meadow"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &canvas = app->canvas();
    // The same canvas, adding light: glows, stars and fireflies.
    tgx::Canvas glow = canvas;
    glow.set_blend(tgx::Blend::additive);

    // Seeded: the same meadow on every run. Grass is drawn back to front.
    tgx::Random random{20261015};
    std::array<Blade, 700> blades{};
    for (std::size_t i = 0; i < blades.size(); ++i) {
        const float shade = random.next_float();
        const float depth = (static_cast<float>(i) + random.next_float()) / static_cast<float>(blades.size());
        blades[i] = {random.next_float(-0.01f, 1.01f), depth, random.next_float(16.f, 34.f),
            random.next_float(-0.15f, 0.15f), random.next_float(0.f, 2.f * pi), 0.f,
            lerp(Color::rgb(0x2c7032), Color::rgb(0x4e963a), shade),
            lerp(Color::rgb(0x88c44e), Color::rgb(0xc6e274), shade)};
    }
    std::array<Firefly, 40> fireflies{};
    for (auto &fly : fireflies) {
        fly = {random.point_in({0.f, 500.f, 1280.f, 200.f}), {}, random.next_float(0.f, 9.f)};
    }

    float hour = 5.5f;
    float rate = base_rate;
    float time = 0.f;
    std::size_t speed = 5;
    bool paused = false;
    std::array<char, 32> label{};

    while (!app->should_close()) {
        app->poll_events();

        const auto &input = app->input();
        const float dt = app->clock().delta();
        time += dt;
        // A wheel notch or an arrow steps the speed of time, Space pauses.
        const int step = (input.wheel().y > 0.f || input.pressed(tgx::Key::right) ? 1 : 0)
            - (input.wheel().y < 0.f || input.pressed(tgx::Key::left) ? 1 : 0);
        speed = static_cast<std::size_t>(std::clamp(static_cast<int>(speed) + step, 0, static_cast<int>(speeds.size()) - 1));
        paused = paused != input.pressed(tgx::Key::space);
        rate += ((paused ? 0.f : speeds[speed] * base_rate) - rate) * std::min(1.f, dt * 3.f);
        hour = tgx::wrap(hour + rate * dt, 24.f);

        const Vec2 size = tgx::to_vec2(canvas.size());
        const float s = size.y / 720.f;
        const Sky sky = sky_at(hour);
        const float altitude = std::sin((hour - 6.f) / 12.f * pi);
        const float day = tgx::smoothstep(-0.1f, 0.3f, altitude);
        const float dark = 1.f - tgx::smoothstep(-0.5f, -0.15f, altitude);
        const Vec2 mouse = input.mouse();
        const bool mouse_in = tgx::contains(tgx::Rect{0.f, 0.f, size.x, size.y}, mouse);
        canvas.rect_gradient({0.f, 0.f, size.x, size.y * 0.7f}, sky.top, sky.bottom);

        // Stars and clouds come from the same seed every frame: no storage.
        tgx::Random sky_random{7};
        for (int i = 0; i < 160; ++i) {
            const Vec2 at = sky_random.point_in({0.f, 0.f, 1.f, 0.62f});
            const float twinkle = 0.65f + 0.35f * std::sin(time * 2.3f + sky_random.next_float(0.f, 40.f));
            const float radius = sky_random.next_float(0.5f, 1.6f) * s;
            glow.circle({at.x * size.x, at.y * size.y}, radius, Color::rgb(0xfff6e0).fade(dark * twinkle * (1.f - at.y)));
        }

        // The sun is up from 5:24 to 19:24, the moon twelve hours later.
        if (const float up = hour - 5.4f; up > 0.f && up < 14.f) {
            const Vec2 sun = on_arc(up, size);
            const float high = tgx::smoothstep(0.f, 0.45f, (size.y * 0.56f - sun.y) / (size.y * 0.5f));
            const Color warm = lerp(Color::rgb(0xff7a3c), Color::rgb(0xfff2c0), high);
            glow.circle_gradient(sun, size.x * 0.45f, warm.fade(0.3f * (1.f - high)), clear);
            glow.circle_gradient(sun, 200.f * s, warm.fade(0.28f), clear);
            glow.circle_gradient(sun, 60.f * s, warm.fade(0.5f), clear);
            canvas.circle(sun, 30.f * s, lerp(Color::rgb(0xffb070), Color::rgb(0xfffbea), high));
        }
        if (const float up = tgx::wrap(hour + 6.6f, 24.f); up < 14.f) {
            const Vec2 moon = on_arc(up, size);
            const float pale = 1.f - 0.6f * day;
            glow.circle_gradient(moon, 160.f * s, Color::rgb(0xb4c2ff).fade(0.2f * dark), clear);
            canvas.circle(moon, 22.f * s, Color::rgb(0xf4f0de).fade(pale));
            for (const auto &crater : craters) {
                canvas.circle(moon + Vec2{crater.x, crater.y} * s, crater.z * s, Color::rgb(0xdcd6c2).fade(pale));
            }
        }

        // Opaque, so the puffs merge; the darker copy below is the shade.
        for (int i = 0; i < 6; ++i) {
            const float k = sky_random.next_float(0.7f, 1.5f) * s;
            const float span = size.x + 300.f * s;
            const float x = tgx::wrap(sky_random.next_float(0.f, span) + time * sky_random.next_float(6.f, 14.f) * s, span);
            const Vec2 c{x - 150.f * s, sky_random.next_float(0.12f, 0.4f) * size.y};
            for (const float lift : {0.f, 5.f}) {
                const Color color = lift == 0.f ? lerp(sky.cloud, sky.top, 0.22f) : sky.cloud;
                canvas.rect_rounded({c.x - 52.f * k, c.y - (6.f + lift) * k, 104.f * k, 20.f * k}, 10.f * k, color);
                for (const auto &puff : puffs) {
                    canvas.circle({c.x + puff.x * k, c.y + (puff.y - lift) * k}, puff.z * k, color);
                }
            }
        }

        // Hills, far to near: lit, then faded into the haze with distance.
        for (std::size_t layer = 0; layer < hill_color.size(); ++layer) {
            const auto tint = [&](Color c) { return lerp(mul(c, sky.light), sky.haze, hill_haze[layer]); };
            const Color top = tint(hill_color[layer]);
            // Mist at the foot of the far hills; the meadow darkens instead.
            draw_hill(canvas, layer, size, top, lerp(top, layer == 3 ? Color::rgb(0x1c3a20) : sky.haze, 0.33f));
            for (const auto &tree : trees) {
                if (tree.layer == layer) {
                    const Vec2 at{tree.u * size.x, hill_y(layer, tree.u * size.x, size) + 4.f * tree.size * s};
                    const Color leaves = tint(Color::rgb(tree.pine ? 0x2e6a48 : 0x3e8a3a));
                    const Color lit = tint(Color::rgb(tree.pine ? 0x3e8058 : 0x62a84a));
                    draw_tree(canvas, at, tree.size * s, tree.pine, leaves, tint(Color::rgb(0x6b4a34)), lit);
                }
            }
        }

        // Grass bent by the wind and the mouse; flowers halfway through it.
        const auto wind = [&](float u, float phase) {
            const float gust = 0.35f * std::pow(0.5f + 0.5f * std::sin(time * 0.31f - u * 4.f), 6.f);
            return 0.12f * std::sin(time * 0.8f + u * 6.f) + 0.06f * std::sin(time * 2.1f + u * 17.f + phase) + gust;
        };
        const auto root_of = [&](float u, float depth) {
            return Vec2{u * size.x, std::lerp(hill_y(3, u * size.x, size) + 4.f * s, size.y + 6.f * s, depth)};
        };
        for (std::size_t i = 0; i < blades.size(); ++i) {
            if (i == blades.size() / 2) {
                tgx::Random flower_random{3};
                for (int f = 0; f < 36; ++f) {
                    const float u = flower_random.next_float();
                    const float depth = flower_random.next_float(0.1f, 0.6f);
                    const float k = s * (0.6f + 0.7f * depth);
                    const Vec2 root = root_of(u, depth);
                    const float stem = flower_random.next_float(20.f, 34.f) * k;
                    const Vec2 head = root + tgx::from_angle(wind(u, 0.f) * 0.7f - pi / 2.f) * stem;
                    const Color petal = mul(flower_random.pick(petals), sky.light);
                    canvas.line(root, head, mul(Color::rgb(0x3c7a34), sky.light), 1.6f * k);
                    for (int j = 0; j < 5; ++j) {
                        const float a = static_cast<float>(j) * 2.f * pi / 5.f + u * 9.f;
                        canvas.circle(head + tgx::from_angle(a) * 3.4f * k, 2.8f * k, petal);
                    }
                    canvas.circle(head, 2.2f * k, mul(Color::rgb(0xf0a830), sky.light));
                }
            }
            Blade &blade = blades[i];
            const float k = s * (0.5f + 0.8f * blade.depth);
            const float length = blade.length * k;
            const Vec2 root = root_of(blade.u, blade.depth);
            float target = blade.lean + wind(blade.u, blade.phase);
            if (const Vec2 away = root + Vec2{0.f, -length * 0.5f} - mouse; mouse_in && tgx::length(away) < 120.f * s) {
                const float push = 1.f - tgx::length(away) / (120.f * s);
                target += (away.x < 0.f ? -1.f : 1.f) * push * push * 1.3f;
            }
            blade.bend += (target - blade.bend) * std::min(1.f, dt * 8.f);
            // Bent more towards the tip; angle 0 is straight up.
            const Vec2 mid = root + tgx::from_angle(blade.bend * 0.45f - pi / 2.f) * length * 0.5f;
            const Vec2 tip = root + tgx::from_angle(blade.bend - pi / 2.f) * length;
            const Vec2 w0{2.2f * k, 0.f}, w1{1.4f * k, 0.f};
            const Color c0 = mul(blade.root, sky.light), c2 = mul(blade.tip, sky.light);
            const Color c1 = lerp(c0, c2, 0.5f);
            canvas.triangle_gradient(root - w0, root + w0, mid + w1, c0, c0, c1);
            canvas.triangle_gradient(root - w0, mid + w1, mid - w1, c0, c1, c1);
            canvas.triangle_gradient(mid - w1, mid + w1, tip, c1, c1, c2);
        }

        // Butterflies by day, flitting over the meadow.
        for (std::size_t i = 1; i < petals.size(); ++i) {
            const auto f = static_cast<float>(i);
            const Vec2 b{size.x * (0.5f + 0.42f * std::sin(time * (0.08f + 0.03f * f) + f * 2.1f)),
                size.y * (0.71f + 0.05f * std::sin(time * (0.4f + 0.1f * f) + f)) + 6.f * s * std::sin(time * 4.f + f)};
            const float w = (4.f + 12.f * std::abs(std::sin(time * 13.f + f * 2.f))) * s;
            const Color color = mul(petals[i], sky.light).fade(day);
            for (const float side : {-w, w}) {
                canvas.triangle(b, b + Vec2{side, -12.f * s}, b + Vec2{side * 1.1f, s}, color);
                canvas.triangle(b, b + Vec2{side * 0.8f, s}, b + Vec2{side * 0.5f, 10.f * s}, color);
            }
            canvas.line(b + Vec2{0.f, -4.f * s}, b + Vec2{0.f, 5.f * s}, Color::rgb(0x2a2420).fade(day), 2.f * s);
        }

        // Fireflies wander low; near the mouse they gather and circle it.
        for (auto &fly : fireflies) {
            Vec2 target = tgx::from_angle(fly.phase * 5.f + 2.5f * std::sin(time * 0.23f + fly.phase)) * 36.f * s;
            target.y += fly.pos.y < size.y * 0.68f ? 30.f * s : fly.pos.y > size.y * 0.95f ? -30.f * s : 0.f;
            if (const Vec2 to = mouse - fly.pos; mouse_in && tgx::length(to) < 280.f * s && tgx::length(to) > 1.f) {
                const Vec2 dir = tgx::normalize(to);
                target += (dir * 90.f + tgx::perpendicular(dir) * 50.f) * s * tgx::smoothstep(40.f * s, 120.f * s, tgx::length(to));
            }
            fly.vel = tgx::lerp(fly.vel, target, std::min(1.f, dt * 1.5f));
            fly.pos += fly.vel * dt;
            fly.pos.x = fly.pos.x < -20.f ? size.x + 20.f : fly.pos.x > size.x + 20.f ? -20.f : fly.pos.x;
            const float pulse = 0.5f + 0.5f * std::sin(time * 2.2f + fly.phase);
            const float light = dark * (0.25f + 0.75f * pulse * pulse);
            glow.circle_gradient(fly.pos, 18.f * s, Color::rgb(0xd4ff6a).fade(0.5f * light), clear);
            glow.circle(fly.pos, 2.f * s, Color::rgb(0xf6ffc8).fade(light));
        }

        // The clock and the speed of time, right, below the fps corner.
        const int minutes = static_cast<int>(hour * 60.f);
        auto end = std::format_to_n(label.data(), label.size(), "{:02}:{:02}", minutes / 60, minutes % 60).out;
        canvas.text({size.x - 24.f, 48.f}, {label.data(), end}, Color::rgb(0xfffaf0), 32.f, {1.f, 0.f});
        end = std::format_to_n(label.data(), label.size(), "{}{:+g}x", paused ? "paused " : "", speeds[speed]).out;
        canvas.text({size.x - 24.f, 84.f}, {label.data(), end}, Color::rgb(0xfffaf0).fade(0.8f), 16.f, {1.f, 0.f});

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
