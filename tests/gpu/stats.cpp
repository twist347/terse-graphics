// Device::stats: the GL work of the last presented frame.

#include "gpu.hpp"

#include <doctest/doctest.h>

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
    CHECK(stats.clears == 0);
    CHECK(stats.shader_switches == 0);
    CHECK(stats.texture_switches == 0);
    CHECK(stats.target_switches == 0);
}
