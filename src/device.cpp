#include "tgx/device.h"

#include "tgx/window.h"

#include <glad/gl.h>

namespace tgx {
    Device::Device(Window &window) noexcept {
        const auto [width, height] = window.framebuffer_size();
        set_viewport(0, 0, width, height);

        glClearColor(m_clear_color.r, m_clear_color.g, m_clear_color.b, m_clear_color.a);
    }

    void Device::set_clear_color(const Color &color) noexcept {
        if (color == m_clear_color) {
            return;
        }

        m_clear_color = color;
        glClearColor(color.r, color.g, color.b, color.a);
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
        const std::array<int, 4> next{x, y, width, height};
        if (next == m_viewport) {
            return;
        }

        m_viewport = next;
        glViewport(x, y, width, height);
    }
}
