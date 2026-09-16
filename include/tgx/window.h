#pragma once

#include "tgx/error.h"

#include <string_view>
#include <utility>

struct GLFWwindow;

namespace tgx {
    class Platform;

    struct WindowParams {
        int width{1920};
        int height{1080};
        const char *title{"tgx"};
        bool vsync{true};
        bool debug_context = true;
    };

    class Window {
    public:
        [[nodiscard]] static auto create(Platform &platform, const WindowParams &params) noexcept -> Result<Window>;

        Window(const Window &) = delete;

        auto operator=(const Window &) -> Window & = delete;

        Window(Window &&other) noexcept;

        auto operator=(Window &&other) noexcept -> Window &;

        ~Window();

        [[nodiscard]] auto should_close() const noexcept -> bool;

        void request_close() noexcept;

        void swap_buffers() noexcept;

        void set_title(std::string_view title) noexcept;

        void set_vsync(bool enabled) noexcept;

        [[nodiscard]] auto framebuffer_size() const noexcept -> std::pair<int, int>;

        [[nodiscard]] auto native_handle() const noexcept -> GLFWwindow * { return m_handle; }

    private:
        explicit Window(GLFWwindow *handle) noexcept : m_handle{handle} {}

        auto destroy() noexcept -> void;

        GLFWwindow *m_handle{nullptr};
    };
}
