#include "tgx/device.h"

#include "tgx/window.h"

#include <cstdint>

#include <glad/gl.h>

namespace {
    [[nodiscard]] constexpr auto to_unit(std::uint8_t channel) noexcept -> float {
        return static_cast<float>(channel) / 255.0F;
    }

    void apply_clear_color(const tgx::Color &color) noexcept {
        glClearColor(to_unit(color.r), to_unit(color.g), to_unit(color.b), to_unit(color.a));
    }
}

namespace tgx {
    Device::Device(Window &window) noexcept {
        const auto [width, height] = window.framebuffer_size();
        set_viewport(0, 0, width, height);

        apply_clear_color(m_clear_color);
    }

    void Device::set_clear_color(const Color &color) noexcept {
        if (color == m_clear_color) {
            return;
        }

        m_clear_color = color;
        apply_clear_color(color);
    }

    void Device::clear(ClearMask mask) noexcept {
        GLbitfield bits = 0;

        if (has(mask, ClearMask::color)) {
            bits |= GL_COLOR_BUFFER_BIT;
        }
        if (has(mask, ClearMask::depth)) {
            bits |= GL_DEPTH_BUFFER_BIT;
        }
        if (has(mask, ClearMask::stencil)) {
            bits |= GL_STENCIL_BUFFER_BIT;
        }

        if (bits != 0) {
            glClear(bits);
        }
    }

    void Device::set_viewport(int x, int y, int width, int height) noexcept {
        const std::array next{x, y, width, height};
        if (next == m_viewport) {
            return;
        }

        m_viewport = next;
        glViewport(x, y, width, height);
    }
}
