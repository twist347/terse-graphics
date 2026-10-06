#pragma once

#include "tgx/blend.h"
#include "tgx/camera.h"
#include "tgx/color.h"
#include "tgx/handle.h"
#include "tgx/math.h"
#include "tgx/render_target.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace tgx {
    namespace gl {
        class Shader;
    }

    namespace detail {
        struct BatchState;
        struct Surface;
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
    // top-left, y down; the canvas is stretched over the whole window, or the
    // part of it set_viewport() gives, so on a scaling display one unit covers
    // several pixels and things keep their size. It follows the window as it
    // is resized. Drawing into a render target instead (set_target), the
    // coordinates are its pixels. A camera (set_camera) moves, turns and
    // zooms the world under them.
    //
    // Shapes are collected by the Device, which draws them before anything
    // else of its own (a draw, a clear) and when the frame is presented, so
    // the picture follows the order of the calls. Shapes in a row with the
    // same target, viewport, size, camera, texture, blend and shader go out
    // as one draw; a change of any of them starts another.
    //
    // Holds no GPU resources of its own, only how to draw: a plain value, to
    // copy for each way of drawing rather than to switch back and forth. All
    // copies draw into the same batch, in the order of the calls:
    //
    //     tgx::Canvas world = app->canvas();
    //     world.set_camera(camera);
    //     world.sprite(player, {pos});            // moves with the camera
    //     app->canvas().rect(bar, colors::red);   // stays put: a HUD
    //
    // It draws through the Device, which must exist while it does.
    class Canvas {
    public:
        // Empty follows what it draws into, as App's canvas does; see
        // set_size().
        [[nodiscard]] static auto create(Size size = {}) noexcept -> Canvas;

        // The area the coordinates span. Empty (the default) is the size of
        // what the canvas covers, all it draws into or its viewport, whatever
        // it is at the time; a size of its own is a fixed logical resolution,
        // stretched over it.
        auto set_size(Size size) noexcept -> void;

        // The area the coordinates span now.
        [[nodiscard]] auto size() const noexcept -> Size;

        // The part of what it draws into the canvas covers, for a minimap or a
        // split screen: in the window's screen coordinates (Window::size()),
        // or a render target's pixels. Empty (the default) is all of it.
        //
        //     tgx::Canvas minimap = app->canvas();
        //     minimap.set_viewport({16, 16, 200, 150});   // its coordinates span 200x150
        auto set_viewport(Rect rect) noexcept -> void;

        [[nodiscard]] auto viewport() const noexcept -> Rect { return m_viewport; }

        // What it draws into: a render target, or nullptr (the default) for
        // the window. Its id and size are taken now, so the target may move
        // afterwards, but it must exist whenever the canvas draws into it.
        //
        //     tgx::Canvas world = app->canvas();
        //     world.set_target(&pixels);   // its coordinates span the target's pixels
        auto set_target(const RenderTarget *target) noexcept -> void;

        // For what comes after; the default shows the world as it is.
        auto set_camera(const Camera2D &camera) noexcept -> void;

        [[nodiscard]] auto camera() const noexcept -> const Camera2D & { return m_camera; }

        // The world point under a point of what it draws into, in the
        // coordinates set_viewport() takes: for a canvas on the window, what
        // is under the mouse. Goes through the viewport, the size and the
        // camera, so it holds for a minimap or a stretched canvas too;
        // Camera2D::to_world alone holds only for a canvas over all of it at
        // its size.
        [[nodiscard]] auto to_world(Vec2 point) const noexcept -> Vec2;

        // Where a world point shows up in what it draws into: the way back
        // from to_world.
        [[nodiscard]] auto to_screen(Vec2 world) const noexcept -> Vec2;

        // For what comes after; Blend::alpha by default.
        auto set_blend(Blend blend) noexcept -> void { m_blend = blend; }

        [[nodiscard]] auto blend() const noexcept -> Blend { return m_blend; }

        // The way out to custom GL: draws what comes after with this shader
        // instead of the built-in one; nullptr goes back to it. Its id is
        // taken now, as set_target() does, so the shader may move afterwards,
        // but it must exist whenever the canvas draws with it. While set, its
        // u_projection and u_texture are the canvas's, written on every draw
        // of it, so do not share it with draws of your own.
        //
        // It takes what the built-in one does:
        //
        //     layout(location = 0) in vec2 in_position;   // canvas coordinates
        //     layout(location = 1) in vec2 in_uv;
        //     layout(location = 2) in vec4 in_color;      // the shape's color or tint
        //     uniform mat4 u_projection;                  // canvas coordinates to clip space
        //     uniform sampler2D u_texture;                // optional; white for shapes, glyphs for text
        //
        // u_texture is its only texture: the canvas fills slot 0 alone, so it
        // may have no other sampler (asserted). Its other uniforms are the
        // caller's to set. Shapes keep the values set when they were added:
        // setting a uniform later, or destroying the shader, draws them first.
        auto set_shader(const gl::Shader *shader) noexcept -> void;

        // Fills all of what it draws into with the color, as the first thing
        // of a frame usually; all of it even for a canvas with a viewport.
        auto clear(Color color) noexcept -> void;

        auto rect(Rect rect, Color color) noexcept -> void;

        // A frame thickness wide, inside the rectangle's edges.
        auto rect_lines(Rect rect, Color color, float thickness = 1.f) noexcept -> void;

        // The color going from top to bottom: a sky, a fade to black.
        auto rect_gradient(Rect rect, Color top, Color bottom) noexcept -> void;

        // A color for each corner, blended between them, clockwise from the
        // top-left; left to right is (left, right, right, left).
        auto rect_gradient(
            Rect rect,
            Color top_left,
            Color top_right,
            Color bottom_right,
            Color bottom_left
        ) noexcept -> void;

        auto triangle(Vec2 a, Vec2 b, Vec2 c, Color color) noexcept -> void;

        // A color for each corner, blended between them: any shape made of
        // triangles can be shaded so.
        auto triangle_gradient(
            Vec2 a, Vec2 b, Vec2 c,
            Color color_a, Color color_b, Color color_c
        ) noexcept -> void;

        // A thickness wide band from a to b, square ends flush with them.
        auto line(Vec2 a, Vec2 b, Color color, float thickness = 1.f) noexcept -> void;

        // Lines from one point to the next, thickness wide, their corners
        // joined without a gap; where they turn back sharply, or a line is too
        // short for its corner, the corner is cut off rather than drawn as a
        // long spike. The two ends are square, as line()'s: ending at the
        // first point again closes the shape, but leaves that one corner
        // unjoined.
        auto line_strip(std::span<const Vec2> points, Color color, float thickness = 1.f) noexcept -> void;

        // As many segments as keep the edge within a quarter pixel of a true
        // circle, so small ones stay cheap and big ones round.
        auto circle(Vec2 center, float radius, Color color) noexcept -> void;

        // inner at the center, blended out to outer at the edge: a glow, a
        // light, a soft shadow (outer see-through).
        auto circle_gradient(Vec2 center, float radius, Color inner, Color outer) noexcept -> void;

        // A ring thickness wide, inside the circle's edge.
        auto circle_lines(Vec2 center, float radius, Color color, float thickness = 1.f) noexcept -> void;

        // The texture is drawn from when the shapes are drawn; if it is
        // updated or destroyed before, they are drawn then. It must not be
        // the texture of the canvas's own render target.
        auto sprite(const Texture &texture, const Sprite &sprite) noexcept -> void;

        // The built-in font's own size, the height of a line: text drawn at it,
        // or at a whole multiple of it, at a whole position, keeps its pixels
        // even squares (on a display scaled by a whole factor).
        static constexpr float default_text_size = 16.f;

        // A line of text, or several split by '\n', its top-left corner at
        // the position; size is the height of a line. The built-in font is a
        // monospaced pixel font with printable ASCII: other characters show
        // as '?'. Drawn in the same draws as the shapes around it.
        auto text(Vec2 position, std::string_view text, Color color, float size = default_text_size) noexcept -> void;

        // The size text takes when drawn: the widest line by the lines' height.
        // A '\n' at the end starts a line with nothing on it yet, which
        // counts, as the place the next character would go.
        [[nodiscard]] static auto measure_text(std::string_view text, float size = default_text_size) noexcept -> Vec2;

        // The frames per second of the Device (Clock::fps) as text, "60 fps",
        // its top-left corner at the position: green at 30 or more, orange at
        // 15 or more, red below.
        auto fps(Vec2 position, float size = default_text_size) noexcept -> void;

    private:
        explicit Canvas(Size size) noexcept;

        // How shapes drawn now with the texture (0: none) go out.
        [[nodiscard]] auto state_for(GlId texture) noexcept -> detail::BatchState;

        auto quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) noexcept -> void;

        // The same with a color for each corner, as two triangles split along
        // corners 0 to 2: blended exactly only when the colors blend flat
        // (rect_gradient sees to that), otherwise the diagonal shows.
        auto quad(const std::array<Vec2, 4> &corners, const std::array<Color, 4> &colors) noexcept -> void;

        // What the canvas draws into.
        [[nodiscard]] auto surface() const noexcept -> detail::Surface;

        // Remakes the transform for the size the canvas has now.
        auto refit() noexcept -> void;

        Size m_size{};
        Rect m_viewport{};
        detail::Target m_target{};
        Camera2D m_camera{};
        Blend m_blend{Blend::alpha};
        // The program of the shader set, 0 for the built-in one.
        GlId m_program{0};
        // The locations of its u_projection, and of its u_texture or -1 when
        // it has none, looked up when it was set.
        std::int32_t m_u_projection{-1};
        std::int32_t m_u_texture{-1};
        // From canvas coordinates to clip space, for the camera and the size
        // it was last made for; remade when either changes.
        Mat4 m_transform{};
        Size m_transform_size{};
    };
}
