#pragma once

#include "tgx/core/clock.hpp"
#include "tgx/core/color.hpp"
#include "tgx/core/error.hpp"
#include "tgx/core/image.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace tgx {
    class RenderTarget;
    class Window;

    namespace gl {
        class Shader;
        class VertexArray;
        struct DrawParams;
    }

    // Empty fields are not cleared. Nothing carries over from one clear to
    // the next.
    //
    //     device.clear({.color = colors::black, .depth = 1.f});
    struct ClearParams {
        // Cleared whole. nullptr: the window.
        const RenderTarget *target{nullptr};
        std::optional<Color> color{};
        // Usually 1, the far end.
        std::optional<float> depth{};
    };

    // The GL work of one frame, from one present() to the next. A switch
    // counts only when it changes something; setting a uniform or updating a
    // texture switches too.
    //
    //     const tgx::FrameStats stats = app->device().stats();
    //     canvas.text({10, 30}, std::format("{} draws", stats.draws), tgx::colors::white);
    struct FrameStats {
        // Draw calls, the Canvas's included.
        int draws{0};
        // Of draws, those of Canvas shapes: one per batch. A batch ends where
        // the target, viewport, size, camera, clip, texture, blend or shader
        // changes.
        int canvas_draws{0};
        int clears{0};
        // Another shader made current.
        int shader_switches{0};
        // A texture slot given another texture.
        int texture_switches{0};
        // Drawing moved between the window and render targets.
        int target_switches{0};
    };

    // The GL context, ready to draw. One Device, for the one window, which
    // must outlive it. GPU resources (Texture, the gl:: types) must be
    // destroyed before it. It holds no other state, so it moves freely.
    //
    // It draws the Canvas's shapes before its own draws, clears, reads and
    // presents, so the picture follows call order. Call flush() before raw
    // GL calls.
    //
    // Raw GL calls go through <glad/gl.h>, which tgx ships and the Device
    // loads. They must leave GL as they found it. The Device does not set
    // again what it knows is set (framebuffer, program, vertex array,
    // textures and active slot, viewport, clear values, render state). It
    // assumes the rest is at GL's defaults (no scissor or stencil test, all
    // color channels written), except that GL_PROGRAM_POINT_SIZE is on.
    //
    // Part of the simple level; only draw() takes tgx::gl types.
    class Device {
    public:
        // Loads GL; on a debug context, logs driver messages. Fails with
        // Error::unsupported below GL 3.3, Error::platform if loading or the
        // Canvas's setup fails, Error::out_of_memory if the GPU is full.
        [[nodiscard]] static auto create(Window &window) -> Result<Device>;

        Device(const Device &) = delete;
        auto operator=(const Device &) -> Device & = delete;

        Device(Device &&other) noexcept : owned_{std::exchange(other.owned_, false)} {
        }

        auto operator=(Device &&other) noexcept -> Device &;

        ~Device();

        auto clear(const ClearParams &params) noexcept -> void;

        // Draws the Canvas's shapes.
        auto flush() noexcept -> void;

        // A screenshot of this frame, opaque. Call it before present(). Waits
        // for the GPU, so not every frame. Empty for a minimized window.
        //
        //     if (input.pressed(tgx::Key::f12) && !app->window().minimized()) {
        //         const auto saved = app->device().read().save("screenshot.png");
        //     }
        [[nodiscard]] auto read() -> Image;

        // Shows the frame, waiting for the display if vsync is on, and
        // records its time for clock(). App::swap_buffers calls this.
        auto present() noexcept -> void;

        // Timing of the frames shown by present().
        [[nodiscard]] auto clock() const noexcept -> const Clock &;

        // Makes the next Clock::delta() count from now, e.g. after loading a
        // level. The pause is left out of delta() and fps(), not elapsed().
        auto restart_clock() noexcept -> void;

        // The frame shown by the last present(). Always counted: a few
        // additions a draw.
        [[nodiscard]] auto stats() const noexcept -> FrameStats;

        // Uses the index buffer if the vertex array has one. Two overloads,
        // since a default argument would need gl::DrawParams defined here.
        auto draw(const gl::Shader &shader, const gl::VertexArray &vertices) noexcept -> void;

        auto draw(
            const gl::Shader &shader,
            const gl::VertexArray &vertices,
            const gl::DrawParams &params
        ) noexcept -> void;

    private:
        Device() noexcept : owned_{true} {
        }

        auto release() noexcept -> void;

        // False after a move.
        bool owned_{false};
    };
}
