#pragma once

#include "tgx/assert.h"
#include "tgx/error.h"
#include "tgx/size.h"

struct GLFWwindow;

namespace tgx {
    class Platform;

    struct WindowParams {
        int width{1280};
        int height{720};
        const char *title{"tgx"};
        bool vsync{true};
        // Debug contexts report driver messages but slow the driver down, so by
        // default only builds with asserts get one. TGX_ENABLE_ASSERTS rather
        // than NDEBUG: it is the same for the library and the app.
        bool debug_context{TGX_ENABLE_ASSERTS != 0};
    };

    // At most one may exist for now: creating a window makes its GL context
    // current.
    class Window {
    public:
        [[nodiscard]] static auto create(Platform &platform, const WindowParams &params) noexcept -> Result<Window>;

        Window(const Window &) = delete;
        auto operator=(const Window &) -> Window & = delete;

        Window(Window &&other) noexcept;
        auto operator=(Window &&other) noexcept -> Window &;

        ~Window();

        [[nodiscard]] auto should_close() const noexcept -> bool;

        auto request_close() noexcept -> void;

        auto swap_buffers() noexcept -> void;

        // Must be nul-terminated UTF-8, as the windowing backend requires.
        auto set_title(const char *title) noexcept -> void;

        auto set_vsync(bool enabled) noexcept -> void;

        // In screen coordinates, the units the OS lays windows out in and the
        // Canvas draws in. The same as framebuffer_size() unless the display
        // scales, e.g. half of it on a Retina screen.
        [[nodiscard]] auto size() const noexcept -> Size;

        // In pixels: what the viewport covers.
        [[nodiscard]] auto framebuffer_size() const noexcept -> Size;

        [[nodiscard]] auto native_handle() const noexcept -> GLFWwindow * { return m_handle; }

    private:
        explicit Window(GLFWwindow *handle) noexcept;

        auto destroy() noexcept -> void;

        GLFWwindow *m_handle{nullptr};
    };
}
