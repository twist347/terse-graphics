#include "tgx/app.h"

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

        return App{std::move(*platform), std::move(*window), std::move(*device)};
    }

    App::App(Platform platform, Window window, Device device) noexcept
        : m_platform{std::move(platform)},
          m_window{std::move(window)},
          m_device{std::move(device)},
          m_framebuffer_size{m_window.framebuffer_size()} {
        // Device::create has already fitted the viewport to this size.
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

        m_platform.poll_events();

        const auto size = m_window.framebuffer_size();
        m_resized = size != m_framebuffer_size;
        if (m_resized) {
            m_framebuffer_size = size;
            m_device.set_viewport(0, 0, size.width, size.height);
        }
    }

    auto App::swap_buffers() noexcept -> void {
        m_window.swap_buffers();
        m_clock.tick();
    }
}
