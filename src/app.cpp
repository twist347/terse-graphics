#include "tgx/app.h"

#include "tgx/size.h"

#include <utility>

namespace tgx {
    auto App::create(const WindowParams &params) noexcept -> Result<App> {
        auto platform = Platform::create();
        if (!platform) {
            return std::unexpected{platform.error()};
        }

        auto window = Window::create(*platform, params);
        if (!window) {
            return std::unexpected{window.error()};
        }

        auto device = Device::create(*window);
        if (!device) {
            return std::unexpected{device.error()};
        }

        return App{std::move(*platform), std::move(*window), std::move(*device), Canvas::create()};
    }

    App::App(Platform platform, Window window, Device device, Canvas canvas) noexcept
        : m_platform{std::move(platform)},
          m_window{std::move(window)},
          m_device{std::move(device)},
          m_canvas{canvas} {
    }

    auto App::should_close() const noexcept -> bool {
        return m_window.should_close();
    }

    auto App::poll_events() noexcept -> void {
        // Started here rather than on creation: resources are loaded between
        // the two, and that time is not a frame.
        if (!m_clock.started()) {
            m_clock.restart();
        }

        // The sizes change only while events are polled, as GLFW reports them.
        const Size framebuffer_size = m_window.framebuffer_size();
        const Size window_size = m_window.size();
        m_platform.poll_events();
        m_resized = m_window.framebuffer_size() != framebuffer_size || m_window.size() != window_size;
    }

    auto App::swap_buffers() noexcept -> void {
        m_device.present();
        m_clock.tick();
    }
}
