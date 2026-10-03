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

        auto device = gl::Device::create(*window);
        if (!device) {
            return std::unexpected{device.error()};
        }

        auto canvas = Canvas::create(window->size());

        return App{std::move(*platform), std::move(*window), std::move(*device), canvas};
    }

    App::App(Platform platform, Window window, gl::Device device, Canvas canvas) noexcept
        : m_platform{std::move(platform)},
          m_window{std::move(window)},
          m_device{std::move(device)},
          m_canvas{std::move(canvas)},
          m_framebuffer_size{m_window.framebuffer_size()},
          m_window_size{m_window.size()} {
        // Device::create and Canvas::create have already been fitted to these.
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

        const Size framebuffer_size = m_window.framebuffer_size();
        const Size window_size = m_window.size();
        m_resized = framebuffer_size != m_framebuffer_size || window_size != m_window_size;
        if (m_resized) {
            m_framebuffer_size = framebuffer_size;
            m_window_size = window_size;
            m_device.set_viewport(framebuffer_size);
            m_canvas.set_size(window_size);
        }
    }

    auto App::swap_buffers() noexcept -> void {
        m_device.present();
        m_clock.tick();
    }
}
