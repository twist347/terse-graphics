#pragma once

#include "tgx/clock.h"
#include "tgx/color.h"
#include "tgx/error.h"
#include "tgx/image.h"

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

    // What a clear resets, and to what; what is left empty stays as it is.
    // Like a draw, every clear states it all, so nothing carries over from
    // one clear to the next.
    //
    //     device.clear({.color = colors::black, .depth = 1.f});
    struct ClearParams {
        // What is cleared, all of it; nullptr is the window.
        const RenderTarget *target{nullptr};
        std::optional<Color> color{};
        // Usually 1, the far end of the depth range.
        std::optional<float> depth{};
        std::optional<std::int32_t> stencil{};
    };

    // The GL context, ready to draw: GPU resources (Texture, gl:: ones) are
    // created while it exists and destroyed before it goes. There is one, of
    // the one window; it only sets the context up and tears it down, which is
    // why it can be moved freely and holds nothing else.
    //
    // It also draws what the Canvas has collected before anything else of its
    // own: a draw, a clear, presenting the frame. So the picture follows the
    // order of the calls. flush() draws it on demand, before raw GL calls.
    // Those must leave GL as they found it: the Device skips setting what it
    // knows to be set (the framebuffer, the program, the vertex array, the
    // textures and the active slot, the viewport, the clear values, the
    // render state) and assumes the rest at GL's defaults (no scissor or
    // stencil test, every color channel written).
    //
    // Belongs to the simple level, as without it no frame is shown; only
    // draw() is of the OpenGL level, and takes its types (tgx/gl.h).
    //
    // It presents the frames of the window it was created for, which must
    // outlive it.
    class Device {
    public:
        // Loads GL functions for the window's context and, on a debug context,
        // routes driver messages to the log. Frames go to this window. Fails
        // with Error::unsupported when the context is older than 3.3,
        // Error::platform when the functions do not load or the Canvas's
        // resources cannot be made, Error::out_of_memory when the GPU has no
        // room for them.
        [[nodiscard]] static auto create(Window &window) -> Result<Device>;

        Device(const Device &) = delete;
        auto operator=(const Device &) -> Device & = delete;

        Device(Device &&other) noexcept : m_owned{std::exchange(other.m_owned, false)} {
        }

        auto operator=(Device &&other) noexcept -> Device &;

        ~Device();

        auto clear(const ClearParams &params) noexcept -> void;

        // Draws what the Canvas has collected.
        auto flush() noexcept -> void;

        // What has been drawn into the window this frame, back on the CPU,
        // rows top to bottom: a screenshot, opaque as the window shows it.
        // Read before present(), which hands the frame to the display and
        // leaves the next one to be drawn anew. Draws what is waiting first
        // and waits for the GPU to finish: not for every frame.
        //
        // A minimized window has nothing to read: the Image is empty.
        //
        //     if (input.pressed(tgx::Key::f12) && !app->window().minimized()) {
        //         const auto saved = app->device().read().save("screenshot.png");
        //     }
        [[nodiscard]] auto read() -> Image;

        // Draws what the Canvas has collected and shows the frame in the
        // window, waiting for the display if the window has vsync on
        // (Window::set_vsync), then times the frame for clock().
        // App::swap_buffers does this.
        auto present() noexcept -> void;

        // The time of the frames present() shows.
        [[nodiscard]] auto clock() const noexcept -> const Clock &;

        // Makes the next Clock::delta() count from now, after a long pause
        // inside the loop such as loading a level, so it does not jump: the
        // time spent is left out of delta() and fps(). elapsed() keeps counting
        // real time.
        auto restart_clock() noexcept -> void;

        // Draws with the index buffer when the vertex array has one, straight
        // from the vertices otherwise. Two overloads rather than a default
        // argument, which would need gl::DrawParams complete here.
        auto draw(const gl::Shader &shader, const gl::VertexArray &vertices) noexcept -> void;

        auto draw(
            const gl::Shader &shader,
            const gl::VertexArray &vertices,
            const gl::DrawParams &params
        ) noexcept -> void;

    private:
        Device() noexcept : m_owned{true} {
        }

        auto release() noexcept -> void;

        // False once moved from: the context is someone else's to tear down.
        bool m_owned{false};
    };
}
