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
          m_device{std::move(device)} {
    }

    auto App::next_frame() noexcept -> bool {
        if (m_in_frame) {
            m_window.swap_buffers();
        }
        m_platform.poll_events();

        if (m_window.should_close()) {
            m_in_frame = false;
            return false;
        }

        m_clock.tick();

        const auto [width, height] = m_window.framebuffer_size();
        m_device.set_viewport(0, 0, width, height);

        m_in_frame = true;
        return true;
    }
}
