#include "tgx/app.h"

#include "tgx/size.h"

#include <utility>

namespace tgx {
    auto App::create(const WindowParams &params) noexcept -> Result<App> {
        auto window = Window::create(params);
        if (!window) {
            return std::unexpected{window.error()};
        }

        auto device = Device::create(*window);
        if (!device) {
            return std::unexpected{device.error()};
        }

        return App{std::move(*window), std::move(*device), Canvas::create()};
    }

    App::App(Window window, Device device, Canvas canvas) noexcept
        : m_window{std::move(window)},
          m_device{std::move(device)},
          m_canvas{canvas} {
    }

    auto App::should_close() const noexcept -> bool {
        return m_window.should_close();
    }

    auto App::poll_events() noexcept -> void {
        // The sizes change only while events are polled, as GLFW reports them.
        const Size framebuffer_size = m_window.framebuffer_size();
        const Size window_size = m_window.size();
        m_window.poll_events();
        m_resized = m_window.framebuffer_size() != framebuffer_size || m_window.size() != window_size;
    }

    auto App::swap_buffers() noexcept -> void {
        m_device.present();
    }
}
