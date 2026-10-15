#include "tgx/core/app.hpp"

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
        return window_.should_close();
    }

    auto App::poll_events() noexcept -> void {
        window_.poll_events();
    }

    auto App::swap_buffers() noexcept -> void {
        device_.present();
    }

    App::App(Window window, Device device, Canvas canvas, Audio audio) noexcept
        : window_{std::move(window)},
          device_{std::move(device)},
          canvas_{canvas},
          audio_{std::move(audio)} {
    }
}
