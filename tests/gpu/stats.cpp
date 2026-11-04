// Device::stats: the GL work of the last presented frame.

#include "gpu.hpp"

#include "gpu/batch.hpp"

#include <doctest/doctest.h>

#include <array>

TEST_CASE("Device::stats: draws, batches, clears and switches of the last frame") {
    auto &device = tgx_test::app().device();
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    const tgx::RenderTarget other = tgx_test::target({2, 2});
    const tgx_test::Cover cover = tgx_test::Cover::create();
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    // Twice the same: the second frame starts from where the first left GL.
    const auto frame = [&] {
        canvas.clear(tgx::colors::black);
        // One batch: same texture, shader and the rest.
        canvas.rect({0, 0, 2, 2}, tgx::colors::red);
        canvas.circle({4, 4}, 2, tgx::colors::blue);
        canvas.text({0, 0}, "a", tgx::colors::white, 4);
        // Another texture: a second batch.
        canvas.sprite(other.texture(), {{6, 6}});
        // Another shader.
        cover.draw({.target = &target, .viewport = tgx::gl::Viewport{0, 0, 1, 1}});
        device.present();
    };
    frame();
    frame();

    const tgx::FrameStats stats = device.stats();
    CHECK(stats.draws == 3);
    CHECK(stats.canvas_draws == 2);
    CHECK(stats.clears == 1);
    // The canvas's shader after cover's, then cover's.
    CHECK(stats.shader_switches == 2);
    // Slot 0: the built-in texture after the sprite's, then the sprite's.
    CHECK(stats.texture_switches == 2);
    // All into the one target, bound since the first frame.
    CHECK(stats.target_switches == 0);
}

TEST_CASE("Device::stats: a frame of nothing counts nothing") {
    auto &device = tgx_test::app().device();
    device.present();
    device.present();

    const tgx::FrameStats stats = device.stats();
    CHECK(stats.draws == 0);
    CHECK(stats.canvas_draws == 0);
    CHECK(stats.canvas_vertices == 0);
    CHECK(stats.canvas_indices == 0);
    CHECK(stats.canvas_upload_bytes == 0);
    CHECK(stats.clears == 0);
    CHECK(stats.shader_switches == 0);
    CHECK(stats.texture_switches == 0);
    CHECK(stats.target_switches == 0);
}

TEST_CASE("Device::stats counts Canvas geometry uploaded by all batches") {
    auto &device = tgx_test::app().device();
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    tgx::Canvas canvas = tgx_test::canvas_on(target);

    canvas.rect({0, 0, 2, 2}, tgx::colors::red);
    device.present();
    tgx::FrameStats stats = device.stats();
    CHECK(stats.canvas_vertices == 4);
    CHECK(stats.canvas_indices == 6);
    CHECK(stats.canvas_upload_bytes == 4 * sizeof(tgx::detail::BatchVertex) + 6 * sizeof(std::uint16_t));

    canvas.rect({0, 0, 2, 2}, tgx::colors::red);
    canvas.rect({2, 2, 2, 2}, tgx::colors::blue);
    device.present();
    stats = device.stats();
    CHECK(stats.canvas_draws == 1);
    CHECK(stats.canvas_vertices == 8);
    CHECK(stats.canvas_indices == 12);
    CHECK(stats.canvas_upload_bytes == 8 * sizeof(tgx::detail::BatchVertex) + 12 * sizeof(std::uint16_t));
}

TEST_CASE("Device::stats counts actual target switches across waiting batches") {
    auto &device = tgx_test::app().device();
    const auto a = tgx_test::target({8, 8});
    const auto b = tgx_test::target({8, 8});
    auto first = tgx_test::canvas_on(a);
    auto second = tgx_test::canvas_on(b);
    device.clear({.target = &a, .color = tgx::colors::black});
    device.present();

    first.rect({0, 0, 8, 8}, tgx::colors::red);
    second.rect({0, 0, 8, 8}, tgx::colors::green);
    first.rect({0, 0, 4, 8}, tgx::colors::blue);
    device.present();
    const auto stats = device.stats();
    CHECK(stats.target_switches == 2);
    CHECK(stats.draws == 3);
    CHECK(stats.canvas_draws == 3);
    const auto image = a.read();
    CHECK(image[1, 4] == tgx::colors::blue);
    CHECK(image[6, 4] == tgx::colors::red);
    CHECK(b.read()[4, 4] == tgx::colors::green);
}

TEST_CASE("Device::stats: lines too short to have a direction draw nothing") {
    auto &device = tgx_test::app().device();
    const tgx::RenderTarget target = tgx_test::target({8, 8});
    tgx::Canvas canvas = tgx_test::canvas_on(target);
    // 1e-30 apart: squared, the distance underflows to 0.
    constexpr std::array close{tgx::Vec2{0, 0}, tgx::Vec2{1e-30f, 0}};

    const auto frame = [&] {
        canvas.line(close[0], close[1], tgx::colors::white, 2);
        canvas.line_strip(close, tgx::colors::white, 2);
        canvas.line_loop(close, tgx::colors::white, 2);
        device.present();
    };
    frame();
    frame();

    CHECK(device.stats().canvas_draws == 0);
}
