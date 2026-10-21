#pragma once

#include "tgx/core/blend.hpp"
#include "tgx/core/camera.hpp"
#include "tgx/core/color.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/math.hpp"
#include "tgx/core/render_target.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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

    // A texture, or a part of it, to draw. Only position is required.
    //
    //     canvas.sprite(player, {pos});
    //     canvas.sprite(atlas, {.position = pos, .size = {64, 64}, .src = {16, 0, 16, 16}});
    struct Sprite {
        // Where origin is placed. By default origin is the top-left corner.
        Vec2 position{};
        // Size on the canvas. Empty: the size of src.
        Vec2 size{};
        // Part of the texture, in texels. Empty: the whole texture. A negative
        // width or height mirrors it.
        Rect src{};
        // Pivot point, from the sprite's top-left corner: placed at position
        // and rotated around. size / 2 is the center.
        Vec2 origin{};
        // Radians, clockwise.
        float rotation{0.f};
        // Multiplies the texture's colors. White keeps them.
        Color tint{colors::white};
    };

    // Widths of a NineSlice's borders, in texels.
    struct Borders {
        float left{0.f};
        float top{0.f};
        float right{0.f};
        float bottom{0.f};
    };

    // A part of a texture cut in nine by its borders, for UI frames that
    // stretch: corners keep their size, edges stretch along, the middle both
    // ways. Zero top and bottom (or left and right) cut it in three.
    //
    //     canvas.nine_slice(frame, {.position = pos, .size = {200, 80}, .borders = {4, 4, 4, 4}});
    struct NineSlice {
        // Where origin is placed. By default origin is the top-left corner.
        Vec2 position{};
        // Size on the canvas. Empty: the size of src. Not mirrored.
        Vec2 size{};
        // Part of the texture, in texels. Empty: the whole texture. Not
        // mirrored.
        Rect src{};
        // Together no wider or taller than src.
        Borders borders{};
        // Size of a border texel on the canvas: 4 for pixel art drawn 4 times
        // as big. Not below 0. Borders that do not fit in size shrink to fit.
        float border_scale{1.f};
        // As in Sprite.
        Vec2 origin{};
        float rotation{0.f};
        Color tint{colors::white};
    };

    // Simple 2D drawing, one call per shape. Shapes are batched and drawn in
    // call order.
    //
    // Coordinates are the window's screen coordinates (Window::size()): (0, 0)
    // at the top-left, y down. The canvas covers the window, or its viewport,
    // and follows resizes; on a scaling display things keep their size. In a
    // render target (set_target) coordinates are its pixels. set_camera()
    // moves, rotates and zooms the world.
    //
    // The Device draws the batch before its own draws and clears, and at
    // present(). Consecutive shapes with the same target, viewport, size,
    // camera, clip, texture, blend and shader become one draw.
    //
    // A Canvas owns no GPU resources, only settings. Copy it for each set of
    // settings instead of switching back and forth; all copies share one batch:
    //
    //     tgx::Canvas world = app->canvas();
    //     world.set_camera(camera);
    //     world.sprite(player, {pos});            // moves with the camera
    //     app->canvas().rect(bar, colors::red);   // stays put: a HUD
    //
    // The Device must exist while the canvas draws.
    class Canvas {
    public:
        // An empty size follows the target, like App's canvas. See set_size().
        [[nodiscard]] static auto create(Size size = {}) noexcept -> Canvas;

        // Size of the coordinate space. Empty (default): the size of the
        // viewport or target. Otherwise a fixed logical resolution, as large
        // as fits in the viewport or target, keeping its shape, centered;
        // the bars on two sides stay as clear() leaves them, and nothing is
        // drawn over them.
        //
        //     app->canvas().set_size({1280, 720});   // the same field in any window
        auto set_size(Size size) noexcept -> void;

        // The current coordinate space. Never empty while there is a target,
        // unless the viewport is empty.
        [[nodiscard]] auto size() const noexcept -> Size;

        // Part of the window or target to cover, e.g. for a minimap. In screen
        // coordinates or target pixels. An empty rect shows nothing;
        // std::nullopt (default) covers all of it.
        //
        //     tgx::Canvas minimap = app->canvas();
        //     minimap.set_viewport(Rect{16, 16, 200, 150});   // its coordinates span 200x150
        //     minimap.set_viewport({});                        // all of it again
        auto set_viewport(std::optional<Rect> rect) noexcept -> void;

        [[nodiscard]] auto viewport() const noexcept -> std::optional<Rect> { return viewport_; }

        // A render target to draw into, or nullptr (default) for the window.
        // Its id and size are copied, so it may be moved, but it must exist
        // while the canvas draws into it.
        //
        //     tgx::Canvas world = app->canvas();
        //     world.set_target(&pixels);   // its coordinates span the target's pixels
        auto set_target(const RenderTarget *target) noexcept -> void;

        // For the shapes after it.
        auto set_camera(const Camera2D &camera) noexcept -> void;

        [[nodiscard]] auto camera() const noexcept -> const Camera2D & { return camera_; }

        // A point of the window or target (in set_viewport() units) in world
        // coordinates, e.g. under the mouse. Accounts for the viewport, size
        // and camera, unlike Camera2D::to_world.
        [[nodiscard]] auto to_world(Vec2 point) const noexcept -> Vec2;

        // The reverse of to_world.
        [[nodiscard]] auto to_screen(Vec2 world) const noexcept -> Vec2;

        // The shapes after it are drawn only inside rect, e.g. a list scrolled
        // inside a panel. In canvas coordinates without the camera, so it
        // stays put and upright as the camera moves. Edges fall on whole
        // pixels. An empty rect shows nothing; std::nullopt (default) clips
        // nothing.
        //
        //     canvas.set_clip(panel);
        //     canvas.text({panel.x, panel.y - scroll}, log, colors::white);   // cut at the panel's edges
        //     canvas.set_clip({});                                            // nothing clipped
        auto set_clip(std::optional<Rect> rect) noexcept -> void;

        [[nodiscard]] auto clip() const noexcept -> std::optional<Rect> { return clip_; }

        // For the shapes after it. Default: Blend::alpha.
        auto set_blend(Blend blend) noexcept -> void { blend_ = blend; }

        [[nodiscard]] auto blend() const noexcept -> Blend { return blend_; }

        // Draws the shapes after it with your shader; nullptr restores the
        // built-in one. Its id is copied, as in set_target(). The canvas sets
        // u_projection and u_texture on every draw, so do not share the shader
        // with your own draws. Its inputs are the built-in shader's:
        //
        //     layout(location = 0) in vec2 in_position;   // canvas coordinates
        //     layout(location = 1) in vec2 in_uv;
        //     layout(location = 2) in vec4 in_color;      // the shape's color or tint
        //     uniform mat4 u_projection;                  // canvas coordinates to clip space
        //     uniform sampler2D u_texture;                // optional; white for shapes, glyphs for text
        //
        // u_texture must be its only sampler (asserted). Other uniforms are
        // yours. Setting one, or destroying the shader, first draws the waiting
        // shapes with the old values.
        auto set_shader(const gl::Shader *shader) noexcept -> void;

        // Fills the whole window or target, ignoring the viewport and the
        // clip.
        auto clear(Color color) noexcept -> void;

        auto rect(Rect rect, Color color) noexcept -> void;

        // Rotated by rotation (radians, clockwise) around origin, which is
        // measured from the top-left corner and placed at rect.x, rect.y, as in
        // Sprite.
        //
        //     canvas.rect({pos.x, pos.y, 40, 20}, colors::red, angle, {20, 10});   // centered on pos
        auto rect(Rect rect, Color color, float rotation, Vec2 origin = {}) noexcept -> void;

        // Outline, drawn inside the edges.
        auto rect_lines(Rect rect, Color color, float thickness = 1.f) noexcept -> void;

        // radius is limited to half the shorter side.
        auto rect_rounded(Rect rect, float radius, Color color) noexcept -> void;

        // Outline, drawn inside the edges.
        auto rect_rounded_lines(Rect rect, float radius, Color color, float thickness = 1.f) noexcept -> void;

        // Vertical gradient.
        auto rect_gradient(Rect rect, Color top, Color bottom) noexcept -> void;

        // Corner colors clockwise from the top-left, blended. Left to right is
        // (left, right, right, left).
        auto rect_gradient(
            Rect rect,
            Color top_left, Color top_right,
            Color bottom_right, Color bottom_left
        ) noexcept -> void;

        auto triangle(Vec2 a, Vec2 b, Vec2 c, Color color) noexcept -> void;

        // A color per corner, blended.
        auto triangle_gradient(
            Vec2 a, Vec2 b, Vec2 c,
            Color color_a, Color color_b, Color color_c
        ) noexcept -> void;

        // Outline, drawn inside the edges, so sharp corners do not stick out.
        auto triangle_lines(Vec2 a, Vec2 b, Vec2 c, Color color, float thickness = 1.f) noexcept -> void;

        // Square ends, exactly at a and b.
        auto line(Vec2 a, Vec2 b, Color color, float thickness = 1.f) noexcept -> void;

        // Corners are joined. One too sharp, or on a too short line, is cut off
        // instead of spiking. Square ends. line_loop() closes the shape.
        auto line_strip(std::span<const Vec2> points, Color color, float thickness = 1.f) noexcept -> void;

        // A closed line_strip, centered on the edges. The first point
        // repeated at the end is skipped, so either way works.
        auto line_loop(std::span<const Vec2> points, Color color, float thickness = 1.f) noexcept -> void;

        // The edge stays within a quarter pixel of a true circle.
        auto circle(Vec2 center, float radius, Color color) noexcept -> void;

        // inner at the center, outer at the edge. A transparent outer gives a
        // glow.
        auto circle_gradient(Vec2 center, float radius, Color inner, Color outer) noexcept -> void;

        // Outline, drawn inside the edge.
        auto circle_lines(Vec2 center, float radius, Color color, float thickness = 1.f) noexcept -> void;

        // From angle start to end: radians, clockwise, 0 pointing right (as
        // from_angle()). Either order; a full turn or more is the whole circle.
        //
        //     canvas.circle_sector(eye, 200, facing - 0.5f, facing + 0.5f, colors::yellow.fade(0.3f));
        auto circle_sector(Vec2 center, float radius, float start, float end, Color color) noexcept -> void;

        // The band between two radii, angles as in circle_sector(): an arc, a
        // progress ring.
        auto ring(
            Vec2 center,
            float inner_radius, float outer_radius,
            float start, float end,
            Color color
        ) noexcept -> void;

        // radii along its own x and y; rotation turns it around the center,
        // as in circle_sector().
        //
        //     canvas.ellipse_lines(planet, {120, 30}, colors::light_gray, 3, -0.3f);  // a tilted ring
        auto ellipse(Vec2 center, Vec2 radii, Color color, float rotation = 0.f) noexcept -> void;

        // Outline, drawn inside the edge, equally thick all round.
        auto ellipse_lines(
            Vec2 center,
            Vec2 radii,
            Color color,
            float thickness = 1.f,
            float rotation = 0.f
        ) noexcept -> void;

        // 3 to 1024 sides, corners on a circle of the radius. rotation is the
        // first corner's angle, as in circle_sector(): 0 puts one on the right.
        auto poly(Vec2 center, int sides, float radius, Color color, float rotation = 0.f) noexcept -> void;

        // Outline, drawn inside the edges.
        auto poly_lines(
            Vec2 center,
            int sides,
            float radius,
            Color color,
            float thickness = 1.f,
            float rotation = 0.f
        ) noexcept -> void;

        // The texture is read when the batch is drawn; updating or destroying
        // it draws the waiting shapes first. It must not be the texture of the
        // canvas's own target.
        auto sprite(const Texture &texture, const Sprite &sprite) noexcept -> void;

        // The texture is read as in sprite().
        auto nine_slice(const Texture &texture, const NineSlice &slice) noexcept -> void;

        // Line height of the built-in font. At this size, or a whole multiple,
        // at whole positions, its pixels stay square.
        static constexpr float default_text_size = 16.f;

        // Top-left corner at position; size is the line height. The built-in
        // font is a monospaced pixel font with printable ASCII: '\n' breaks the
        // line, '\t' goes to the next 4-column stop, '\r' is skipped, other
        // characters show as '?'.
        //
        // anchor is the point of the text placed at position, as a fraction
        // of its size: {0.5f, 0.5f} centers it, {1, 0} puts its top-right
        // corner there. Each line is aligned by its own width. The shift is
        // rounded to whole units, so the pixels stay square.
        //
        //     canvas.text(screen.center(), "GAME OVER", colors::red, 48, {0.5f, 0.5f});
        auto text(
            Vec2 position,
            std::string_view text,
            Color color,
            float size = default_text_size,
            Vec2 anchor = {}
        ) noexcept -> void;

        // Widest line by total height. "" is one line tall; a trailing '\n'
        // adds one.
        [[nodiscard]] static auto measure_text(std::string_view text, float size = default_text_size) noexcept -> Vec2;

        // "60 fps" at position: green at 30 or more, orange at 15 or more, red
        // below.
        auto fps(Vec2 position, float size = default_text_size) noexcept -> void;

    private:
        explicit Canvas(Size size) noexcept;

        // Batch settings for a shape drawn now with the texture (0: none).
        [[nodiscard]] auto state_for(GlId texture) noexcept -> detail::BatchState;

        auto quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) noexcept -> void;

        // Two triangles split along corners 0-2: exact only when the colors
        // blend flat (rect_gradient checks), otherwise the diagonal shows.
        auto quad(const std::array<Vec2, 4> &corners, const std::array<Color, 4> &colors) noexcept -> void;

        // closed connects the last point to the first.
        auto strip(std::span<const Vec2> points, Color color, float thickness, bool closed) noexcept -> void;

        // For a circle of the radius drawn with the state: the edge within a
        // quarter pixel. Takes state from state_for(), which updates the
        // size this reads.
        [[nodiscard]] auto segments(float radius, const detail::BatchState &state) const noexcept -> std::size_t;

        // The window or target it draws into.
        [[nodiscard]] auto surface() const noexcept -> detail::Surface;

        // Rebuilds transform_ for the size and the camera.
        auto refit(Size span) noexcept -> void;

        Size size_{};
        std::optional<Rect> viewport_{};
        detail::Target target_{};
        Camera2D camera_{};
        std::optional<Rect> clip_{};
        Blend blend_{Blend::alpha};
        // 0: the built-in shader.
        GlId program_{0};
        // Looked up in set_shader(). u_texture_ is -1 when there is none.
        std::int32_t u_projection_{-1};
        std::int32_t u_texture_{-1};
        // Canvas coordinates to clip space. Rebuilt at the next shape when
        // stale or when the size changed (e.g. a resized window).
        Mat4 transform_{};
        Size transform_size_{};
        bool transform_stale_{true};
    };
}
