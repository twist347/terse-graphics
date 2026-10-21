// Arkanoid: A/D or Left/Right moves the paddle, Space launches the ball. Where
// the ball lands on the paddle sets its angle. Grey bricks take two hits.
// Three lives; P pauses, Enter starts again once the game is won or lost.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <format>
#include <print>
#include <vector>

namespace {
    constexpr tgx::Size field_size{.width = 960, .height = 720};
    constexpr auto field_width = static_cast<float>(field_size.width);
    constexpr auto field_height = static_cast<float>(field_size.height);
    constexpr tgx::Vec2 field_center{field_width / 2, field_height / 2};

    constexpr float paddle_width = 120;
    constexpr float paddle_height = 16;
    constexpr float paddle_y = field_height - 50;
    constexpr float paddle_speed = 720;
    constexpr tgx::Color paddle_color = tgx::colors::light_gray;

    constexpr float ball_radius = 8;
    constexpr float ball_start_speed = 420;
    constexpr float ball_speedup = 1.02f;
    constexpr float ball_max_speed = 850;
    // From straight up, at the paddle's ends.
    constexpr float ball_max_bounce_angle = tgx::radians(60);
    constexpr tgx::Color ball_color = tgx::colors::white;

    // The longest step one update takes. With ball_max_speed it keeps the ball
    // from passing a brick in one step: 850 / 30 < brick height + diameter.
    constexpr float max_step = 1.f / 30;

    constexpr int columns = 12;
    constexpr int rows = 6;
    constexpr float brick_width = 72;
    constexpr float brick_height = 26;
    constexpr float brick_gap = 4;
    constexpr float bricks_top = 90;
    constexpr float bricks_left = (field_width - columns * brick_width - (columns - 1) * brick_gap) / 2;

    // Top to bottom; the first row takes two hits.
    constexpr std::array<tgx::Color, rows> row_colors{
        tgx::colors::gray, tgx::colors::red, tgx::colors::orange,
        tgx::colors::yellow, tgx::colors::green, tgx::colors::cyan,
    };
    constexpr std::array<int, rows> row_points{50, 40, 30, 20, 10, 10};

    constexpr int start_lives = 3;

    constexpr tgx::Color bg_color{16, 16, 24};
    // Around the field when the window is not its shape.
    constexpr tgx::Color bar_color = tgx::colors::black;

    [[nodiscard]] auto beep(float frequency, float duration) -> tgx::Sound {
        return tgx::Sound::tone({
            .waveform = tgx::Waveform::square, .frequency = frequency, .duration = duration, .volume = 0.15f,
        });
    }

    struct Sounds {
        tgx::Sound paddle = beep(330, 0.06f);
        tgx::Sound wall = beep(220, 0.03f);
        // Played higher for higher rows.
        tgx::Sound brick = beep(440, 0.05f);
        tgx::Sound lost = beep(110, 0.4f);
    };

    // Panned from the left edge of the field to the right one.
    auto play(tgx::Audio &audio, const tgx::Sound &sound, float x, float pitch = 1) -> void {
        const float pan = std::clamp(tgx::remap(x, 0, field_width, -1, 1), -1.f, 1.f);
        audio.play(sound, {.pan = pan, .pitch = pitch});
    }

    struct Brick {
        tgx::Rect body{};
        int row{};
        int hits{};

        auto draw(tgx::Canvas &canvas) const -> void {
            // A cracked one is darker.
            const tgx::Color color = tgx::lerp(row_colors[row], tgx::colors::black, hits == 1 && row == 0 ? 0.4f : 0.f);
            canvas.rect(body, color);
            canvas.rect({body.x, body.y, body.width, 4}, tgx::lerp(color, tgx::colors::white, 0.4f));
        }
    };

    struct Ball {
        tgx::Circle body{};
        tgx::Vec2 vel{};

        // What it hit, to sound.
        enum class Hit { none, wall, out };

        auto move(float dt) -> Hit {
            body.center += vel * dt;
            const float r = body.radius;

            if (body.center.x - r < 0) {
                body.center.x = r;
                vel.x = std::abs(vel.x);
                return Hit::wall;
            }
            if (body.center.x + r > field_width) {
                body.center.x = field_width - r;
                vel.x = -std::abs(vel.x);
                return Hit::wall;
            }
            if (body.center.y - r < 0) {
                body.center.y = r;
                vel.y = std::abs(vel.y);
                return Hit::wall;
            }
            if (body.center.y - r > field_height) {
                return Hit::out;
            }
            return Hit::none;
        }

        // Up, at an angle set by where it lands: the middle sends it straight
        // up, the ends at ball_max_bounce_angle. True if it hit.
        auto bounce_off(tgx::Rect paddle) -> bool {
            if (vel.y < 0 || !tgx::overlaps(paddle, body)) {
                return false;
            }
            const float offset = std::clamp((body.center.x - paddle.center().x) / (paddle.width / 2), -1.f, 1.f);
            const float speed = std::min(tgx::length(vel) * ball_speedup, ball_max_speed);
            vel = tgx::from_angle(tgx::radians(-90) + offset * ball_max_bounce_angle) * speed;
            body.center.y = paddle.y - body.radius;
            return true;
        }

        // Off the side it came in from: the one it is least deep in. True if
        // it hit; not while already leaving, so a brick is hit once.
        auto bounce_off(const Brick &brick) -> bool {
            if (!tgx::overlaps(brick.body, body)) {
                return false;
            }
            const auto box = tgx::Rect::from_center(body.center, {body.radius * 2, body.radius * 2});
            const tgx::Rect overlap = tgx::intersection(brick.body, box);
            const tgx::Vec2 away = body.center - brick.body.center();
            float &speed = overlap.width < overlap.height ? vel.x : vel.y;
            const float side = overlap.width < overlap.height ? away.x : away.y;
            if (speed * side > 0) {
                return false;
            }
            speed = -speed;
            return true;
        }

        auto draw(tgx::Canvas &canvas) const -> void {
            canvas.circle(body.center, body.radius, ball_color);
        }
    };

    struct Game {
        tgx::Rect paddle{field_center.x - paddle_width / 2, paddle_y, paddle_width, paddle_height};
        Ball ball{{{}, ball_radius}};
        // On the paddle until launched.
        bool held = true;
        std::vector<Brick> bricks = make_bricks();
        int left = rows * columns;
        int lives = start_lives;
        int score = 0;
        bool paused = false;

        [[nodiscard]] static auto make_bricks() -> std::vector<Brick> {
            std::vector<Brick> bricks;
            for (int row = 0; row < rows; ++row) {
                for (int column = 0; column < columns; ++column) {
                    const tgx::Rect body{
                        bricks_left + static_cast<float>(column) * (brick_width + brick_gap),
                        bricks_top + static_cast<float>(row) * (brick_height + brick_gap),
                        brick_width, brick_height,
                    };
                    bricks.push_back({body, row, row == 0 ? 2 : 1});
                }
            }
            return bricks;
        }

        [[nodiscard]] auto won() const -> bool { return left == 0; }

        [[nodiscard]] auto lost() const -> bool { return lives == 0; }

        auto update(const tgx::Input &input, tgx::Audio &audio, const Sounds &sounds, float dt) -> void {
            if (won() || lost()) {
                if (input.pressed(tgx::Key::enter)) {
                    *this = Game{};
                }
                return;
            }
            if (input.pressed(tgx::Key::p)) {
                paused = !paused;
            }
            if (paused) {
                return;
            }

            const float axis = std::clamp(
                input.axis(tgx::Key::left, tgx::Key::right) + input.axis(tgx::Key::a, tgx::Key::d), -1.f, 1.f
            );
            paddle.x = std::clamp(paddle.x + axis * paddle_speed * dt, 0.f, field_width - paddle.width);

            if (held) {
                ball.body.center = {paddle.center().x, paddle.y - ball.body.radius};
                if (input.pressed(tgx::Key::space)) {
                    held = false;
                    ball.vel = tgx::from_angle(tgx::radians(-90)) * ball_start_speed;
                }
                return;
            }

            const float x = ball.body.center.x;
            switch (ball.move(dt)) {
                case Ball::Hit::wall: play(audio, sounds.wall, x);
                    break;
                case Ball::Hit::out: lose_ball(audio, sounds);
                    return;
                case Ball::Hit::none: break;
            }
            if (ball.bounce_off(paddle)) {
                play(audio, sounds.paddle, x);
            }
            for (Brick &brick: bricks) {
                if (brick.hits > 0 && ball.bounce_off(brick)) {
                    hit(brick, audio, sounds);
                }
            }
        }

        auto hit(Brick &brick, tgx::Audio &audio, const Sounds &sounds) -> void {
            play(audio, sounds.brick, brick.body.center().x, 1.f + static_cast<float>(rows - brick.row) * 0.1f);
            if (--brick.hits == 0) {
                score += row_points[brick.row];
                --left;
            }
        }

        auto lose_ball(tgx::Audio &audio, const Sounds &sounds) -> void {
            play(audio, sounds.lost, ball.body.center.x);
            --lives;
            held = true;
            ball.vel = {};
        }

        auto draw(tgx::Canvas &canvas) const -> void {
            canvas.clear(bar_color);
            canvas.rect({0, 0, field_width, field_height}, bg_color);

            for (const Brick &brick: bricks) {
                if (brick.hits > 0) {
                    brick.draw(canvas);
                }
            }
            canvas.rect_rounded(paddle, paddle.height / 2, paddle_color);
            ball.draw(canvas);

            canvas.text({20, 20}, std::format("SCORE {}", score), tgx::colors::white, 32);
            for (int i = 0; i < lives; ++i) {
                canvas.circle({field_width - 30 - static_cast<float>(i) * 28, 36}, ball_radius, ball_color);
            }

            constexpr float message_size = 48;
            constexpr tgx::Vec2 below = field_center + tgx::Vec2{0, 70};
            if (won() || lost()) {
                canvas.text(below, won() ? "CLEARED" : "GAME OVER", tgx::colors::white, message_size, {0.5f, 1});
                canvas.text(below + tgx::Vec2{0, 16}, "Enter: play again", tgx::colors::gray, 24, {0.5f, 0});
            } else if (paused) {
                canvas.text(below, "PAUSED", tgx::colors::white, message_size, {0.5f, 0.5f});
            } else if (held) {
                canvas.text(below, "Space: launch", tgx::colors::gray, 24, {0.5f, 0.5f});
            }
        }
    };
}

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - games 02 arkanoid"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &canvas = app->canvas();
    // The same field in any window: as large as fits, with bars on two sides.
    canvas.set_size(field_size);
    const Sounds sounds{};
    Game game{};

    while (!app->should_close()) {
        app->poll_events();

        // A stall (a dragged window, a hiccup) is not one huge step through a brick.
        const float dt = std::min(app->clock().delta(), max_step);
        game.update(app->input(), app->audio(), sounds, dt);
        game.draw(canvas);
        canvas.fps({10, field_height - 26});

        app->swap_buffers();
    }

    return 0;
}
