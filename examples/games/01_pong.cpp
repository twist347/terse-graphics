// Pong to 7 points: W/S moves the left paddle; the right one is a bot, or
// with Tab a second player on Up/Down. Space pauses; Enter starts a new match
// once one is won. The field keeps its shape at any window size.

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

    struct Ball {
        tgx::Circle body;
        tgx::Vec2 vel;
        tgx::Color color;

        void update(float dt) {
            body.center += vel * dt;

            // Off the top or bottom: back on the field, heading away from the edge.
            if (body.center.y - body.radius < 0) {
                body.center.y = body.radius;
                vel.y = std::abs(vel.y);
            } else if (body.center.y + body.radius > field_height) {
                body.center.y = field_height - body.radius;
                vel.y = -std::abs(vel.y);
            }
        }

        void draw(tgx::Canvas &canvas) const {
            canvas.circle(body.center, body.radius, color);
        }

        // From where it stands, toward -1 (left) or 1 (right), at a random angle.
        void serve(float toward, tgx::Random &random) {
            const float angle = random.next_float(-ball_serve_angle, ball_serve_angle);
            tgx::Vec2 dir = tgx::from_angle(angle);
            dir.x *= toward;
            vel = dir * ball_serve_speed;
        }

        void bounce_off(tgx::Rect paddle) {
            const float away = body.center.x < paddle.center().x ? -1.f : 1.f;
            if (!tgx::overlaps(paddle, body) || vel.x * away > 0) {
                return;
            }

            const float offset = std::clamp((body.center.y - paddle.center().y) / (paddle.height / 2), -1.f, 1.f);
            const float angle = offset * ball_max_bounce_angle;
            const float speed = std::min(tgx::length(vel) * ball_speedup, ball_max_speed);

            tgx::Vec2 dir = tgx::from_angle(angle);
            dir.x *= away;
            vel = dir * speed;
            // Out of the paddle, so the next frame does not hit again.
            body.center.x = away < 0 ? paddle.x - body.radius : paddle.right() + body.radius;
        }
    };

    struct Paddle {
        tgx::Rect body;
        tgx::Color color;
        tgx::Key up;
        tgx::Key down;
        bool bot = false;

        void update(const tgx::Input &input, const Ball &ball, float dt) {
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

        void draw(tgx::Canvas &canvas) const {
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
        tgx::Random random;

        void update(const tgx::Input &input, float dt) {
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
            ball.update(dt);
            ball.bounce_off(left.body);
            ball.bounce_off(right.body);

            // Past a side, wholly: a point to the other player, the next serve
            // toward the one who missed.
            if (ball.body.center.x < -ball.body.radius) {
                goal(right_score, -1);
            } else if (ball.body.center.x > field_width + ball.body.radius) {
                goal(left_score, 1);
            }
        }

        void goal(int &score, float toward) {
            ++score;
            ball.body.center = field_center;
            ball.vel = {};
            serve_toward = toward;
            serve_timer.reset();
        }

        [[nodiscard]] auto over() const -> bool {
            return left_score == winning_score || right_score == winning_score;
        }

        void draw(tgx::Canvas &canvas) const {
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

        static void draw_net(tgx::Canvas &canvas) {
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
    Game game{};

    while (!app->should_close()) {
        app->poll_events();

        // A stall (a dragged window, a hiccup) is not one huge step through a paddle.
        const float dt = std::min(app->clock().delta(), max_step);
        game.update(app->input(), dt);
        game.draw(canvas);
        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
