// Pong to 7 points: W/S moves the left paddle; the right one is a bot, or
// with Tab a second player on Up/Down. Space pauses; Enter starts a new match
// once one is won. The field keeps its shape at any window size. Beeps for
// paddle, wall and point are made in code and heard from where the ball is.

#include "tgx/tgx.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <print>
#include <string>

namespace {
    constexpr tgx::Size field_size{.width = 1280, .height = 720};
    constexpr auto field_width = static_cast<float>(field_size.width);
    constexpr auto field_height = static_cast<float>(field_size.height);
    constexpr tgx::Vec2 field_center{field_width / 2, field_height / 2};

    constexpr float paddle_width = 16;
    constexpr float paddle_height = 110;
    constexpr float paddle_margin = 40;
    constexpr float paddle_speed = 520;
    constexpr float paddle_y = (field_height - paddle_height) / 2;
    constexpr tgx::Color paddle_color = tgx::colors::white;

    // Slower than a player, so it can be beaten.
    constexpr float bot_speed = 380;
    // How far off the ball it may be before it moves, so it does not jitter.
    constexpr float bot_dead_zone = 8;

    constexpr float ball_radius = 10;
    constexpr float ball_serve_speed = 450;
    constexpr float ball_serve_angle = tgx::radians(30);
    constexpr float ball_speedup = 1.06f;
    constexpr float ball_max_speed = 1000;
    // The longest step one update takes. With ball_max_speed it keeps the ball
    // from passing a paddle in one step: 1000 / 30 < paddle width + diameter.
    constexpr float max_step = 1.f / 30;
    constexpr float ball_max_bounce_angle = tgx::radians(60);
    constexpr tgx::Color ball_color = tgx::colors::red;

    constexpr float serve_delay = 1;
    constexpr int winning_score = 7;

    constexpr float net_width = 4;
    constexpr tgx::Color net_color = tgx::colors::gray;
    constexpr tgx::Color score_color = tgx::colors::gray;
    constexpr tgx::Color bg_color{16, 16, 24};
    // Around the field when the window is not its shape.
    constexpr tgx::Color bar_color = tgx::colors::black;

    // A square wave, as the arcade one had.
    [[nodiscard]] auto beep(float frequency, float duration) -> tgx::Sound {
        return tgx::Sound::tone({
            .waveform = tgx::Waveform::square, .frequency = frequency, .duration = duration, .volume = 0.2f,
        });
    }

    // The pitches and lengths of the 1972 arcade Pong.
    struct Sounds {
        tgx::Sound paddle = beep(459, 0.096f);
        tgx::Sound wall = beep(226, 0.016f);
        tgx::Sound point = beep(490, 0.257f);
    };

    // Panned from the left edge of the field to the right one.
    auto play(tgx::Audio &audio, const tgx::Sound &sound, float x) -> void {
        const float pan = std::clamp(x / field_width * 2 - 1, -1.f, 1.f);
        audio.play(sound, {.pan = pan});
    }

    struct Ball {
        tgx::Circle body{};
        tgx::Vec2 vel{};
        tgx::Color color{};

        // True if it hit the top or bottom.
        auto update(float dt) -> bool {
            body.center += vel * dt;

            // Off the top or bottom: back on the field, heading away from the edge.
            if (body.center.y - body.radius < 0) {
                body.center.y = body.radius;
                vel.y = std::abs(vel.y);
                return true;
            }
            if (body.center.y + body.radius > field_height) {
                body.center.y = field_height - body.radius;
                vel.y = -std::abs(vel.y);
                return true;
            }
            return false;
        }

        auto draw(tgx::Canvas &canvas) const -> void {
            canvas.circle(body.center, body.radius, color);
        }

        // From where it stands, toward -1 (left) or 1 (right), at a random angle.
        auto serve(float toward, tgx::Random &random) -> void {
            const float angle = random.next_float(-ball_serve_angle, ball_serve_angle);
            tgx::Vec2 dir = tgx::from_angle(angle);
            dir.x *= toward;
            vel = dir * ball_serve_speed;
        }

        // True if it hit the paddle.
        auto bounce_off(tgx::Rect paddle) -> bool {
            const float away = body.center.x < paddle.center().x ? -1.f : 1.f;
            if (!tgx::overlaps(paddle, body) || vel.x * away > 0) {
                return false;
            }

            const float offset = std::clamp((body.center.y - paddle.center().y) / (paddle.height / 2), -1.f, 1.f);
            const float angle = offset * ball_max_bounce_angle;
            const float speed = std::min(tgx::length(vel) * ball_speedup, ball_max_speed);

            tgx::Vec2 dir = tgx::from_angle(angle);
            dir.x *= away;
            vel = dir * speed;
            // Out of the paddle, so the next frame does not hit again.
            body.center.x = away < 0 ? paddle.x - body.radius : paddle.right() + body.radius;
            return true;
        }
    };

    struct Paddle {
        tgx::Rect body{};
        tgx::Color color{};
        tgx::Key up{};
        tgx::Key down{};
        bool bot = false;

        auto update(const tgx::Input &input, const Ball &ball, float dt) -> void {
            const float axis = bot ? follow(ball) : input.axis(up, down);
            const float speed = bot ? bot_speed : paddle_speed;
            body.y = std::clamp(body.y + axis * speed * dt, 0.f, field_height - body.height);
        }

        // The bot's keys: toward the ball while it comes, back to the middle
        // while it goes.
        [[nodiscard]] auto follow(const Ball &ball) const -> float {
            const bool coming = (ball.vel.x > 0) == (body.center().x > field_width / 2);
            const float target = coming ? ball.body.center.y : field_height / 2;
            const float gap = target - body.center().y;
            if (std::abs(gap) < bot_dead_zone) {
                return 0;
            }
            return gap > 0 ? 1.f : -1.f;
        }

        auto draw(tgx::Canvas &canvas) const -> void {
            canvas.rect(body, color);
        }
    };

    struct Game {
        Paddle left{
            {paddle_margin, paddle_y, paddle_width, paddle_height},
            paddle_color,
            tgx::Key::w, tgx::Key::s
        };
        Paddle right{
            {field_width - paddle_margin - paddle_width, paddle_y, paddle_width, paddle_height},
            paddle_color,
            tgx::Key::up, tgx::Key::down,
            true,
        };

        Ball ball{{field_center, ball_radius}, {}, ball_color};

        int left_score = 0;
        int right_score = 0;
        // The ball waits in the middle, then goes toward serve_toward.
        tgx::Timer serve_timer = tgx::Timer::once(serve_delay);
        float serve_toward = 1;
        bool paused = false;
        tgx::Random random{};

        auto update(const tgx::Input &input, tgx::Audio &audio, const Sounds &sounds, float dt) -> void {
            if (over()) {
                if (input.pressed(tgx::Key::enter)) {
                    // A new match, against whom the last one was.
                    const bool bot = right.bot;
                    *this = Game{};
                    right.bot = bot;
                }
                return;
            }
            if (input.pressed(tgx::Key::space)) {
                paused = !paused;
            }
            if (input.pressed(tgx::Key::tab)) {
                right.bot = !right.bot;
            }
            if (paused) {
                return;
            }

            left.update(input, ball, dt);
            right.update(input, ball, dt);
            if (serve_timer.tick(dt)) {
                ball.serve(serve_toward, random);
            }
            if (ball.update(dt)) {
                play(audio, sounds.wall, ball.body.center.x);
            }
            if (ball.bounce_off(left.body) || ball.bounce_off(right.body)) {
                play(audio, sounds.paddle, ball.body.center.x);
            }

            // Past a side, wholly: a point to the other player, the next serve
            // toward the one who missed.
            const float x = ball.body.center.x;
            if (x < -ball.body.radius) {
                play(audio, sounds.point, x);
                goal(right_score, -1);
            } else if (x > field_width + ball.body.radius) {
                play(audio, sounds.point, x);
                goal(left_score, 1);
            }
        }

        auto goal(int &score, float toward) -> void {
            ++score;
            ball.body.center = field_center;
            ball.vel = {};
            serve_toward = toward;
            serve_timer.reset();
        }

        [[nodiscard]] auto over() const -> bool {
            return left_score == winning_score || right_score == winning_score;
        }

        auto draw(tgx::Canvas &canvas) const -> void {
            canvas.clear(bar_color);
            canvas.rect({0, 0, field_width, field_height}, bg_color);
            left.draw(canvas);
            right.draw(canvas);
            draw_net(canvas);
            ball.draw(canvas);

            constexpr float score_size = 64;
            constexpr float score_gap = 60;
            constexpr float score_top = 30;
            canvas.text({field_width / 2 - score_gap, score_top}, std::to_string(left_score), score_color, score_size,
                        {1, 0});
            canvas.text({field_width / 2 + score_gap, score_top}, std::to_string(right_score), score_color, score_size);

            constexpr float message_size = 48;

            if (over()) {
                const char *winner = left_score > right_score ? "LEFT WINS" : "RIGHT WINS";
                canvas.text(field_center, winner, tgx::colors::white, message_size, {0.5f, 1});
                canvas.text(field_center + tgx::Vec2{0, 16}, "Enter: play again", tgx::colors::gray, 24, {0.5f, 0});
            } else if (paused) {
                canvas.text(field_center, "PAUSED", tgx::colors::white, message_size, {0.5f, 0.5f});
            }

            const char *players = right.bot ? "vs bot - Tab: 2 players" : "2 players - Tab: vs bot";
            canvas.text({10, field_height - 10}, players, tgx::colors::gray, 16, {0, 1});
        }

        static auto draw_net(tgx::Canvas &canvas) -> void {
            constexpr float dash = 20;
            for (float y = dash / 2; y < field_height; y += dash * 2) {
                canvas.rect({field_width / 2 - net_width / 2, y, net_width, dash}, net_color);
            }
        }
    };
}

int main() {
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - games 01 pong"});
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

        // A stall (a dragged window, a hiccup) is not one huge step through a paddle.
        const float dt = std::min(app->clock().delta(), max_step);
        game.update(app->input(), app->audio(), sounds, dt);
        game.draw(canvas);
        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
