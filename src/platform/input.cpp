#include "tgx/core/input.hpp"

#include "tgx/core/assert.hpp"

#include "base/utf8.hpp"
#include "platform/input_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace {
    // Per key or button. Edges are stored, not derived from down, so a tap
    // shorter than a frame still shows.
    enum : std::uint8_t {
        bit_down = 1,
        bit_pressed = 2,
        bit_released = 4,
        // Pressed or OS-repeated since the previous poll.
        bit_repeated = 8,
    };

    [[nodiscard]] constexpr auto has_flag(std::uint8_t bits, std::uint8_t flag) noexcept -> bool {
        return (bits & flag) != 0;
    }

    // Asserted: a value cast from outside the enum would read past the
    // array.
    [[nodiscard]] auto index_of(tgx::Key key) noexcept -> std::size_t {
        TGX_ASSERT_MSG(std::to_underlying(key) < tgx::key_count, "key {}: not a Key", std::to_underlying(key));

        return std::to_underlying(key);
    }

    [[nodiscard]] auto index_of(tgx::MouseButton button) noexcept -> std::size_t {
        TGX_ASSERT_MSG(
            std::to_underlying(button) < tgx::mouse_button_count,
            "mouse button {}: not a MouseButton",
            std::to_underlying(button)
        );

        return std::to_underlying(button);
    }

    // A repeat is not a press and does not change down. Only repeated() sees
    // it.
    auto set_edge(std::uint8_t &bits, tgx::detail::InputAction action) noexcept -> void {
        using enum tgx::detail::InputAction;
        if (action == press) {
            bits |= bit_down | bit_pressed | bit_repeated;
        } else if (action == repeat) {
            bits |= bit_repeated;
        } else if (action == release) {
            bits = static_cast<std::uint8_t>((bits & ~bit_down) | bit_released);
        }
    }
}

namespace tgx {
    auto Input::down(Key key) const noexcept -> bool {
        return has_flag(keys_[index_of(key)], bit_down);
    }

    auto Input::pressed(Key key) const noexcept -> bool {
        return has_flag(keys_[index_of(key)], bit_pressed);
    }

    auto Input::released(Key key) const noexcept -> bool {
        return has_flag(keys_[index_of(key)], bit_released);
    }

    auto Input::repeated(Key key) const noexcept -> bool {
        return has_flag(keys_[index_of(key)], bit_repeated);
    }

    auto Input::axis(Key negative, Key positive) const noexcept -> float {
        return detail::unit_axis(down(negative), down(positive));
    }

    auto Input::direction() const noexcept -> Vec2 {
        const auto held = [this](Key a, Key b) noexcept { return down(a) || down(b); };
        return detail::unit_direction(
            held(Key::a, Key::left), held(Key::d, Key::right),
            held(Key::w, Key::up), held(Key::s, Key::down)
        );
    }

    auto Input::direction(Key left, Key right, Key up, Key down) const noexcept -> Vec2 {
        return detail::unit_direction(this->down(left), this->down(right), this->down(up), this->down(down));
    }

    auto Input::down(MouseButton button) const noexcept -> bool {
        return has_flag(buttons_[index_of(button)], bit_down);
    }

    auto Input::pressed(MouseButton button) const noexcept -> bool {
        return has_flag(buttons_[index_of(button)], bit_pressed);
    }

    auto Input::released(MouseButton button) const noexcept -> bool {
        return has_flag(buttons_[index_of(button)], bit_released);
    }

    auto Input::mouse() const noexcept -> Vec2 {
        return mouse_;
    }

    auto Input::mouse_delta() const noexcept -> Vec2 {
        return mouse_delta_;
    }

    auto Input::wheel() const noexcept -> Vec2 {
        return wheel_;
    }

    auto Input::text() const noexcept -> std::string_view {
        return text_;
    }

    auto detail::InputAccess::reset(Input &input, Vec2 mouse) noexcept -> void {
        input.keys_ = {};
        input.buttons_ = {};
        input.mouse_ = mouse;
        input.mouse_delta_ = {};
        input.settling_ = false;
        input.wheel_ = {};
        input.text_.clear();
    }

    auto detail::InputAccess::key(Input &input, Key key, InputAction action) noexcept -> void {
        if (key != Key::unknown) {
            set_edge(input.keys_[index_of(key)], action);
        }
    }

    auto detail::InputAccess::button(Input &input, MouseButton button, InputAction action) noexcept -> void {
        set_edge(input.buttons_[index_of(button)], action);
    }

    auto detail::InputAccess::cursor(Input &input, Vec2 position) noexcept -> void {
        if (!input.settling_) {
            input.mouse_delta_ += position - input.mouse_;
        }
        input.mouse_ = position;
    }

    auto detail::InputAccess::scroll(Input &input, Vec2 offset) noexcept -> void {
        input.wheel_ += offset;
    }

    auto detail::InputAccess::character(Input &input, char32_t codepoint) -> void {
        append_utf8(input.text_, codepoint);
    }

    auto detail::InputAccess::settle_mouse(Input &input, Vec2 position) noexcept -> void {
        input.mouse_ = position;
        input.settling_ = true;
    }

    auto detail::InputAccess::begin_frame(Input &input) noexcept -> void {
        // Held keys and buttons stay down.
        for (std::uint8_t &bits: input.keys_) {
            bits &= bit_down;
        }
        for (std::uint8_t &bits: input.buttons_) {
            bits &= bit_down;
        }
        input.mouse_delta_ = {};
        input.wheel_ = {};
        input.text_.clear();
    }

    auto detail::InputAccess::end_frame(Input &input) noexcept -> void {
        input.settling_ = false;
    }
}
