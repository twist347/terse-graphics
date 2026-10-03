#pragma once

#include "tgx/blend.h"
#include "tgx/camera.h"
#include "tgx/color.h"
#include "tgx/math.h"
#include "tgx/size.h"

namespace tgx {
    class Texture;

    namespace gl {
        class Shader;

        namespace detail {
            struct BatchState;
        }
    }

    // A texture, or a part of one, put on the canvas. Only the position is
    // needed: by default it is the whole texture at its own size.
    //
    //     canvas.sprite(player, {pos});
    //     canvas.sprite(atlas, {.position = pos, .size = {64, 64}, .src = {16, 0, 16, 16}});
    struct Sprite {
        // Where origin lands; with the default origin, the top-left corner.
        Vec2 position{};
        // On the canvas. Empty (the default) is the size of src.
        Vec2 size{};
        // The part of the texture, in texels from its top-left corner. Empty
        // (the default) is the whole texture. A negative width or height
        // mirrors it: {0, 0, -16, 16} is the same 16x16 texels, flipped.
        Rect src{};
        // The point of the sprite, from its top-left corner, that sits at
        // position and that rotation turns around: {0, 0} is the corner, half
        // of size the middle.
        Vec2 origin{};
        // Radians, clockwise on screen.
        float rotation{0.f};
        // Multiplies the texture's colors; white leaves them as they are.
        Color tint{colors::white};
    };

    // Simple 2D drawing: shapes go in with one call each and are drawn
    // together, in as few draws as the GPU allows.
    //
    // Coordinates are screen coordinates (Window::size()), (0, 0) at the
    // top-left, y down; the canvas is stretched over the whole viewport, so on
    // a scaling display one unit covers several pixels and things keep their
    // size. A camera (set_camera) moves, turns and zooms the world under them.
    //
    // Shapes are collected by the Device, which draws them before anything
    // else of its own (a draw, a clear, a viewport change) and when the frame
    // is presented, so the picture follows the order of the calls. Shapes in a row with the same
    // texture, camera, blend and shader go out as one draw; a change of any of
    // them starts another.
    //
    // Holds no GPU resources of its own, only how to draw: a plain value. It
    // needs the Device to exist to be created and drawn with (asserted).
    class Canvas {
    public:
        [[nodiscard]] static auto create(Size size) noexcept -> Canvas;

        // The area the coordinates span, usually Window::size(). App keeps its
        // own canvas at the window's size.
        auto set_size(Size size) noexcept -> void;

        [[nodiscard]] auto size() const noexcept -> Size { return m_size; }

        // For what comes after; the default shows the world as it is. Set it
        // back to Camera2D{} for things that stay put on screen, like a HUD.
        auto set_camera(const Camera2D &camera) noexcept -> void;

        [[nodiscard]] auto camera() const noexcept -> const Camera2D & { return m_camera; }

        // For what comes after; Blend::alpha by default.
        auto set_blend(Blend blend) noexcept -> void { m_blend = blend; }

        [[nodiscard]] auto blend() const noexcept -> Blend { return m_blend; }

        // The way out to custom GL: draws what comes after with this shader
        // instead of the built-in one; nullptr goes back to it. The shader is
        // borrowed: it must outlive being set here, not only the shapes drawn
        // with it. While set, its u_projection and u_texture are the canvas's
        // (it writes them on every draw), so do not share it with draws of
        // your own.
        //
        // It takes what the built-in one does:
        //
        //     layout(location = 0) in vec2 in_position;   // canvas coordinates
        //     layout(location = 1) in vec2 in_uv;
        //     layout(location = 2) in vec4 in_color;      // the shape's color or tint
        //     uniform mat4 u_projection;                  // canvas coordinates to clip space
        //     uniform sampler2D u_texture;                // optional; white for shapes
        //
        // Its other uniforms are the caller's to set. Shapes keep the values
        // set when they were added: setting a uniform later, or moving or
        // destroying the shader, draws them first.
        auto set_shader(gl::Shader *shader) noexcept -> void;

        [[nodiscard]] auto shader() const noexcept -> gl::Shader * { return m_shader; }

        // Fills the whole framebuffer with the color, as the first thing of a
        // frame usually.
        auto clear(Color color) noexcept -> void;

        auto rect(Rect rect, Color color) noexcept -> void;

        // A frame thickness wide, inside the rectangle's edges.
        auto rect_lines(Rect rect, Color color, float thickness = 1.f) noexcept -> void;

        auto triangle(Vec2 a, Vec2 b, Vec2 c, Color color) noexcept -> void;

        // A thickness wide band from a to b, square ends flush with them.
        auto line(Vec2 a, Vec2 b, Color color, float thickness = 1.f) noexcept -> void;

        // As many segments as keep the edge within a quarter unit of a true
        // circle, so small ones stay cheap and big ones round.
        auto circle(Vec2 center, float radius, Color color) noexcept -> void;

        // A ring thickness wide, inside the circle's edge.
        auto circle_lines(Vec2 center, float radius, Color color, float thickness = 1.f) noexcept -> void;

        // The texture is drawn from when the shapes are drawn; if it is moved
        // or destroyed before, they are drawn then.
        auto sprite(const Texture &texture, const Sprite &sprite) noexcept -> void;

        // Draws what has been collected; the same as Device::flush(). Only
        // needed before raw GL calls.
        auto flush() noexcept -> void;

    private:
        explicit Canvas(Size size) noexcept;

        // How shapes drawn now with the texture (nullptr: none) go out.
        [[nodiscard]] auto state_for(const Texture *texture) const noexcept -> gl::detail::BatchState;

        auto quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) noexcept -> void;

        Size m_size{};
        Camera2D m_camera{};
        Blend m_blend{Blend::alpha};
        // nullptr for the built-in one.
        gl::Shader *m_shader{nullptr};
        // From canvas coordinates to clip space, kept in step with the size
        // and the camera.
        Mat4 m_transform{};
    };
}
