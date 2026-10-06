#include "tgx/app.h"

#include <utility>

namespace tgx {
    auto App::create(const WindowParams &params) -> Result<App> {
        auto window = Window::create(params);
        if (!window) {
            return std::unexpected{window.error()};
        }

        auto device = Device::create(*window);
        if (!device) {
            return std::unexpected{device.error()};
        }

        return App{std::move(*window), std::move(*device), Canvas::create(), Audio::create()};
    }

    auto App::should_close() const noexcept -> bool {
        return m_window.should_close();
    }

    auto App::poll_events() noexcept -> void {
        m_window.poll_events();
    }

    auto App::swap_buffers() noexcept -> void {
        m_device.present();
    }

    App::App(Window window, Device device, Canvas canvas, Audio audio) noexcept
        : m_window{std::move(window)},
          m_device{std::move(device)},
          m_canvas{canvas},
          m_audio{std::move(audio)} {
    }
}
