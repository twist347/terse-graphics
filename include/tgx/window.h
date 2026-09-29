#pragma once

#include "tgx/error.h"
#include "tgx/size.h"

struct GLFWwindow;

namespace tgx {
    class Platform;

    // Signature-compatible with glfwGetProcAddress and glad's GLADloadfunc.
    using GlProc = void (*)();
    using GlLoader = GlProc (*)(const char *name);

    struct WindowParams {
        int width{1280};
        int height{720};
        const char *title{"tgx"};
        bool vsync{true};
        // Debug contexts report driver messages but slow the driver down, so by
        // default only debug builds of the app get one.
#if defined(NDEBUG)
        bool debug_context{false};
#else
        bool debug_context{true};
#endif
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

        [[nodiscard]] auto framebuffer_size() const noexcept -> Size;

        // Resolves GL functions for this window's context; it must be current.
        [[nodiscard]] auto gl_loader() const noexcept -> GlLoader;

        [[nodiscard]] auto native_handle() const noexcept -> GLFWwindow * { return m_handle; }

    private:
        explicit Window(GLFWwindow *handle) noexcept;

        auto destroy() noexcept -> void;

        GLFWwindow *m_handle{nullptr};
    };
}
