// A school of fish in deep water. Each fish keeps its distance, swims along
// with its neighbors and stays near them, so they gather into shoals and
// streams. A fish is tinted by its heading: a shoal turning together changes
// color as one. Three glowing orbs drift through and the fish flow around
// them. Neighbors are found through a grid of cells, so thousands stay fast.
//
// Hold the left mouse button to lure the fish into a swirl around the
// cursor, the right one to scatter them. Up / down: more or fewer fish.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <format>
#include <numbers>
#include <print>
#include <string_view>
#include <vector>

namespace {
    constexpr float pi = std::numbers::pi_v<float>;

    struct Fish {
        tgx::Vec2 position{};
        tgx::Vec2 velocity{};
    };

    constexpr float sight = 40.f;
    constexpr float personal_space = 13.f;
    constexpr float min_speed = 70.f;
    constexpr float max_speed = 150.f;
    // Followed; enough for a shoal. All too close are still kept apart.
    constexpr std::size_t max_neighbors = 24;

    // Square cells one sight wide: a fish sees only into the 3x3 cells
    // around its own. Fish are sorted by cell into one array, reused.
    struct Grid {
        std::size_t columns{1};
        std::size_t rows{1};
        // Cell c holds order[starts[c]] up to order[starts[c + 1]].
        std::vector<std::size_t> starts{};
        std::vector<std::size_t> order{};
        std::vector<std::size_t> cell_of{};

        [[nodiscard]] auto column(float x) const noexcept -> std::size_t {
            return static_cast<std::size_t>(std::clamp(x / sight, 0.f, static_cast<float>(columns - 1)));
        }

        [[nodiscard]] auto row(float y) const noexcept -> std::size_t {
            return static_cast<std::size_t>(std::clamp(y / sight, 0.f, static_cast<float>(rows - 1)));
        }

        auto build(const std::vector<Fish> &fish, tgx::Size area) -> void {
            columns = static_cast<std::size_t>(std::max(area.width, 1)) / static_cast<std::size_t>(sight) + 1;
            rows = static_cast<std::size_t>(std::max(area.height, 1)) / static_cast<std::size_t>(sight) + 1;
            const std::size_t cells = columns * rows;
            starts.assign(cells + 1, 0);
            order.resize(fish.size());
            cell_of.resize(fish.size());
            for (std::size_t i = 0; i < fish.size(); ++i) {
                cell_of[i] = row(fish[i].position.y) * columns + column(fish[i].position.x);
                ++starts[cell_of[i]];
            }
            // Each start becomes its cell's end, then counts back down.
            for (std::size_t c = 1; c < cells; ++c) {
                starts[c] += starts[c - 1];
            }
            starts[cells] = fish.size();
            for (std::size_t i = 0; i < fish.size(); ++i) {
                order[--starts[cell_of[i]]] = i;
            }
        }
    };

    // Heading to color, around the circle: neighbors close, so no hue
    // passes through gray.
    constexpr std::array palette{
        tgx::Color::rgb(0x4cc9f0), tgx::Color::rgb(0x80ffdb), tgx::Color::rgb(0xffd166),
        tgx::Color::rgb(0xff8c7a), tgx::Color::rgb(0xff7eb6), tgx::Color::rgb(0xa78bfa),
    };

    auto heading_color(tgx::Vec2 velocity) -> tgx::Color {
        const float t = (tgx::angle(velocity) + pi) / (2.f * pi) * static_cast<float>(palette.size());
        const auto index = static_cast<std::size_t>(t) % palette.size();
        return tgx::lerp(palette[index], palette[(index + 1) % palette.size()], t - std::floor(t));
    }

    auto spawn(tgx::Random &random, tgx::Size area) -> Fish {
        const tgx::Rect where{0.f, 0.f, static_cast<float>(area.width), static_cast<float>(area.height)};
        return {random.point_in(where), random.direction() * random.next_float(min_speed, max_speed)};
    }

    // An orb's center: a slow loop through the window.
    auto orb_center(std::size_t i, float time, tgx::Vec2 area) -> tgx::Vec2 {
        const float k = static_cast<float>(i);
        const float x = 0.5f + 0.36f * std::sin(time * (0.05f + 0.013f * k) + k * 2.1f);
        const float y = 0.5f + 0.32f * std::cos(time * (0.07f - 0.011f * k) + k * 1.7f);
        return {x * area.x, y * area.y};
    }
}

int main() {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - scenes 05 flock"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &canvas = app->canvas();
    const auto &input = app->input();

    constexpr std::size_t start_count = 1500;
    constexpr std::size_t count_step = 250;
    constexpr std::size_t max_count = 4000;
    constexpr std::array orb_radii{46.f, 30.f, 62.f};
    constexpr std::size_t mote_count = 160;

    tgx::Random random{2024};
    std::vector<Fish> fish{};
    fish.reserve(max_count);
    for (std::size_t i = 0; i < start_count; ++i) {
        fish.push_back(spawn(random, canvas.size()));
    }
    std::vector<tgx::Vec2> pushes(max_count);
    Grid grid{};

    // Specks drifting up in the light, in fractions of the window.
    std::array<tgx::Vec3, mote_count> motes{};
    for (auto &mote : motes) {
        mote = {random.next_float(), random.next_float(), random.next_float(0.4f, 1.f)};
    }

    // Eased in and out, so the ring and the pull fade rather than snap.
    float lure = 0.f;
    float scare = 0.f;
    std::array<char, 96> hud{};

    while (!app->should_close()) {
        app->poll_events();

        const float dt = std::min(app->clock().delta(), 1.f / 30.f);
        const auto time = static_cast<float>(app->clock().elapsed());
        const tgx::Size size = canvas.size();
        const tgx::Vec2 area = tgx::to_vec2(size);
        const tgx::Vec2 mouse = input.mouse();

        if (input.repeated(tgx::Key::up) && fish.size() < max_count) {
            for (std::size_t i = 0; i < count_step; ++i) {
                fish.push_back(spawn(random, size));
            }
        }
        if (input.repeated(tgx::Key::down) && fish.size() > count_step) {
            fish.resize(fish.size() - count_step);
        }
        const auto approach = [dt](float value, bool on) { return std::clamp(value + (on ? 3.f : -2.f) * dt, 0.f, 1.f); };
        lure = approach(lure, input.down(tgx::MouseButton::left));
        scare = approach(scare, input.down(tgx::MouseButton::right));
        const float lure_power = tgx::ease::in_out_sine(lure);
        const float scare_power = tgx::ease::in_out_sine(scare);

        std::array<tgx::Vec2, orb_radii.size()> orbs;
        for (std::size_t o = 0; o < orbs.size(); ++o) {
            orbs[o] = orb_center(o, time, area);
        }

        // Steering: all fish look at the same frame, then all move.
        grid.build(fish, size);
        for (std::size_t i = 0; i < fish.size(); ++i) {
            const Fish &self = fish[i];
            tgx::Vec2 offsets{};
            tgx::Vec2 velocities{};
            tgx::Vec2 apart{};
            std::size_t seen = 0;
            const std::size_t column = grid.column(self.position.x);
            const std::size_t row = grid.row(self.position.y);
            for (std::size_t r = row > 0 ? row - 1 : 0; r <= std::min(row + 1, grid.rows - 1); ++r) {
                for (std::size_t c = column > 0 ? column - 1 : 0; c <= std::min(column + 1, grid.columns - 1); ++c) {
                    const std::size_t cell = r * grid.columns + c;
                    for (std::size_t k = grid.starts[cell]; k < grid.starts[cell + 1]; ++k) {
                        const Fish &other = fish[grid.order[k]];
                        const tgx::Vec2 to = other.position - self.position;
                        const float d2 = tgx::length_squared(to);
                        if (d2 > sight * sight || d2 < 0.01f) {
                            continue;
                        }
                        if (++seen <= max_neighbors) {
                            offsets += to;
                            velocities += other.velocity;
                        }
                        if (d2 < personal_space * personal_space) {
                            apart -= to * (personal_space / d2);
                        }
                    }
                }
            }

            tgx::Vec2 push = apart * 90.f;
            if (seen > 0) {
                const float n = static_cast<float>(std::min(seen, max_neighbors));
                push += (velocities / n - self.velocity) * 2.4f + offsets / n * 2.f;
            }
            // A slow private wander, so a lone fish still meanders.
            push += tgx::from_angle(time * 0.6f + static_cast<float>(i) * 2.399f) * 10.f;

            // Turn away from the edges.
            constexpr float margin = 100.f;
            constexpr float turn = 5.f;
            push.x += (std::max(margin - self.position.x, 0.f) - std::max(self.position.x - area.x + margin, 0.f)) * turn;
            push.y += (std::max(margin - self.position.y, 0.f) - std::max(self.position.y - area.y + margin, 0.f)) * turn;

            for (std::size_t o = 0; o < orbs.size(); ++o) {
                const tgx::Vec2 from = self.position - orbs[o];
                const float reach = orb_radii[o] + 40.f;
                const float d = tgx::length(from);
                if (d < reach && d > 0.01f) {
                    push += from / d * (reach - d) * 14.f;
                }
            }

            const tgx::Vec2 to_mouse = mouse - self.position;
            const float d = tgx::length(to_mouse);
            if (d > 0.01f) {
                const tgx::Vec2 toward = to_mouse / d;
                // Lured: pulled onto a circle around the cursor, and around it.
                if (lure_power > 0.f && d < 360.f) {
                    push += (toward * (d - 80.f) * 2.5f + tgx::perpendicular(toward) * 220.f) * lure_power;
                }
                if (scare_power > 0.f && d < 220.f) {
                    push -= toward * (220.f - d) * 16.f * scare_power;
                }
            }
            pushes[i] = push;
        }

        for (std::size_t i = 0; i < fish.size(); ++i) {
            Fish &f = fish[i];
            f.velocity += pushes[i] * dt;
            const float speed = tgx::length(f.velocity);
            if (speed > 0.f) {
                f.velocity *= std::clamp(speed, min_speed, max_speed) / speed;
            }
            f.position += f.velocity * dt;
            // Left outside by a shrinking window: back in on the other side.
            f.position.x = std::fmod(f.position.x + area.x, area.x);
            f.position.y = std::fmod(f.position.y + area.y, area.y);
        }

        // Deep water, lighter near the surface.
        canvas.clear(tgx::Color::rgb(0x030a18));
        canvas.rect_gradient({0.f, 0.f, area.x, area.y}, tgx::Color::rgb(0x0f4c64), tgx::Color::rgb(0x030a18));

        canvas.set_blend(tgx::Blend::additive);
        // Shafts of light from the surface, swaying.
        constexpr std::size_t shafts = 5;
        for (std::size_t s = 0; s < shafts; ++s) {
            const float k = static_cast<float>(s);
            const float x = area.x * (0.12f + 0.19f * k) + std::sin(time * 0.15f + k * 1.9f) * 50.f;
            const float half = 30.f + 18.f * std::sin(k * 3.7f);
            const tgx::Vec2 top_left{x - half, 0.f};
            const tgx::Vec2 bottom{x - area.y * 0.35f, area.y * 0.9f};
            const tgx::Color light = tgx::Color::rgb(0x9fe7ff).fade(0.07f + 0.03f * std::sin(time * 0.4f + k));
            const tgx::Color none = light.fade(0.f);
            canvas.triangle_gradient(top_left, {x + half, 0.f}, bottom + tgx::Vec2{half * 3.f, 0.f}, light, light, none);
            canvas.triangle_gradient(top_left, bottom + tgx::Vec2{half * 3.f, 0.f}, bottom, light, none, none);
        }
        for (auto &mote : motes) {
            mote.y -= 0.006f * mote.z * dt;
            if (mote.y < 0.f) {
                mote.y += 1.f;
            }
            const tgx::Vec2 at{(mote.x + 0.01f * std::sin(time * 0.5f + mote.z * 20.f)) * area.x, mote.y * area.y};
            canvas.circle(at, 1.2f * mote.z, tgx::Color::rgb(0xbfefff).fade(0.25f * mote.z));
        }

        for (std::size_t o = 0; o < orbs.size(); ++o) {
            const float r = orb_radii[o];
            const float breath = 1.f + 0.06f * std::sin(time * 1.3f + static_cast<float>(o) * 2.f);
            constexpr tgx::Color glow = tgx::Color::rgb(0xb388ff);
            canvas.circle_gradient(orbs[o], r * 2.6f * breath, glow.fade(0.22f), glow.fade(0.f));
            canvas.circle_gradient(orbs[o], r * breath, tgx::Color::rgb(0xf0e6ff).fade(0.3f), glow.fade(0.08f));
            canvas.circle_lines(orbs[o], r * breath, tgx::Color::rgb(0xe0d0ff).fade(0.5f), 1.5f);
        }

        // Faint wakes, adding up where a shoal is dense.
        for (const Fish &f : fish) {
            const tgx::Vec2 back = f.velocity * 0.11f;
            canvas.line(f.position - back, f.position, heading_color(f.velocity).fade(0.22f), 2.f);
        }

        canvas.set_blend(tgx::Blend::alpha);
        for (const Fish &f : fish) {
            const float speed = tgx::length(f.velocity);
            const tgx::Vec2 ahead = f.velocity / speed;
            const tgx::Vec2 side = tgx::perpendicular(ahead);
            const float fast = (speed - min_speed) / (max_speed - min_speed);
            const tgx::Color color = tgx::lerp(heading_color(f.velocity), tgx::colors::white, 0.3f * fast);
            canvas.triangle(f.position + ahead * 7.f, f.position - ahead * 5.f - side * 3.f,
                            f.position - ahead * 5.f + side * 3.f, color);
        }

        // The cursor's ring: breathing at rest, wide and cyan when luring,
        // ripples outward when scattering.
        const float pulse = tgx::ease::in_out_sine(std::abs(std::fmod(time * 0.8f, 2.f) - 1.f));
        canvas.set_blend(tgx::Blend::additive);
        // Not over the HUD corner, where a cursor outside the window reads 0, 0.
        const float rest = 1.f - std::max(lure_power, scare_power);
        if (rest > 0.f && (mouse.x > 640.f || mouse.y > 64.f)) {
            const float r = 16.f + 4.f * pulse;
            canvas.circle_lines(mouse, r + 1.f, tgx::colors::white.fade(0.35f * rest), 2.f);
        }
        if (lure_power > 0.f) {
            const float r = 80.f + 8.f * pulse;
            constexpr tgx::Color cyan = tgx::Color::rgb(0x6fe7ff);
            canvas.circle_lines(mouse, r, cyan.fade(0.08f * lure_power), 10.f);
            canvas.circle_lines(mouse, r + 1.5f, cyan.fade(0.6f * lure_power), 3.f);
        }
        if (scare_power > 0.f) {
            constexpr tgx::Color coral = tgx::Color::rgb(0xffc2b0);
            for (std::size_t w = 0; w < 3; ++w) {
                const float t = std::fmod(time * 0.9f + static_cast<float>(w) / 3.f, 1.f);
                const float r = 20.f + 200.f * tgx::ease::out_cubic(t);
                canvas.circle_lines(mouse, r + 2.f, coral.fade(0.7f * (1.f - t) * scare_power), 4.f);
            }
        }
        canvas.set_blend(tgx::Blend::alpha);

        const auto written = std::format_to_n(
            hud.data(), static_cast<std::ptrdiff_t>(hud.size()),
            "{} fish   up/down: more/fewer   left mouse: lure   right mouse: scatter", fish.size()
        );
        const auto length = static_cast<std::size_t>(std::min(written.size, static_cast<std::ptrdiff_t>(hud.size())));
        const std::string_view line{hud.data(), length};
        const tgx::Vec2 extent = tgx::Canvas::measure_text(line);
        canvas.rect_rounded({4.f, 4.f, extent.x + 12.f, extent.y + 42.f}, 6.f, tgx::colors::black.fade(0.35f));
        canvas.text({10.f, 40.f}, line, tgx::Color::rgb(0xa8dadc).fade(0.8f));

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
