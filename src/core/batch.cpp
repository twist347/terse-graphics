#include "core/batch.hpp"

#include "tgx/core/assert.hpp"
#include "tgx/core/image.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/version.hpp"

#include "core/context.hpp"
#include "core/default_font.hpp"
#include "gl/shader_internal.hpp"

#include <glad/gl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
    // The default font, white on transparent, and the white block shapes
    // sample.
    [[nodiscard]] auto builtin_atlas() -> tgx::Image {
        namespace font = tgx::detail::default_font;

        auto image = tgx::Image::create({font::atlas_width, font::atlas_height});
        for (int y = 0; y < font::atlas_height; ++y) {
            for (int x = 0; x < font::atlas_width; ++x) {
                const auto byte = font::bits[static_cast<std::size_t>(y * font::row_bytes + x / 8)];
                if ((byte & (0x80 >> (x % 8))) != 0) {
                    image[x, y] = tgx::colors::white;
                }
            }
        }
        return image;
    }

    constexpr const char *vertex_source = TGX_GLSL_VERSION R"(
        layout(location = 0) in vec2 in_position;
        layout(location = 1) in vec2 in_uv;
        layout(location = 2) in vec4 in_color;

        uniform mat4 u_projection;

        out vec2 uv;
        out vec4 color;

        void main() {
            gl_Position = u_projection * vec4(in_position, 0.0, 1.0);
            uv = in_uv;
            color = in_color;
        }
    )";

    constexpr const char *fragment_source = TGX_GLSL_VERSION R"(
        in vec2 uv;
        in vec4 color;

        uniform sampler2D u_texture;

        out vec4 out_color;

        void main() {
            out_color = texture(u_texture, uv) * color;
        }
    )";

    using tgx::detail::batch_max_indices;
    using tgx::detail::batch_max_vertices;

    static_assert(batch_max_vertices - 1 <= std::numeric_limits<std::uint16_t>::max());
}

namespace tgx::detail {
    auto Batch::create() -> Result<Batch> {
        auto shader = gl::Shader::from_source(vertex_source, fragment_source);
        if (!shader) {
            // Tested on every supported platform; a driver rejecting it is
            // the platform's fault.
            TGX_ASSERT_MSG(false, "the 2D batch shader failed: {}", shader.error());
            return std::unexpected{Error::platform};
        }
        const std::int32_t u_projection = gl::detail::location(shader->uniform<Mat4>("u_projection"));
        shader->set(shader->uniform<gl::TextureSlot>("u_texture"), {0});

        auto vertex_buffer = gl::Buffer<BatchVertex>::create(batch_max_vertices);
        if (!vertex_buffer) {
            return std::unexpected{vertex_buffer.error()};
        }
        auto index_buffer = gl::Buffer<std::uint16_t>::create(batch_max_indices);
        if (!index_buffer) {
            return std::unexpected{index_buffer.error()};
        }

        const std::array layout{
            gl::VertexAttribute::of(0, &BatchVertex::position),
            gl::VertexAttribute::of(1, &BatchVertex::uv),
            gl::VertexAttribute::of(2, &BatchVertex::color),
        };
        auto vertex_array = gl::VertexArray::create<BatchVertex>(layout);
        vertex_array.set_vertex_buffer(*vertex_buffer);
        vertex_array.set_index_buffer(*index_buffer);

        auto builtin = Texture::create(builtin_atlas(), {.filter = TextureFilter::nearest});
        if (!builtin) {
            return std::unexpected{builtin.error()};
        }

        return Batch{
            std::move(*shader),
            u_projection,
            std::move(*vertex_buffer),
            std::move(*index_buffer),
            std::move(vertex_array),
            std::move(*builtin),
        };
    }

    auto Batch::reserve(
        Context &context,
        const BatchState &state,
        std::size_t vertex_count,
        std::size_t index_count
    ) noexcept -> std::uint16_t {
        TGX_ASSERT(vertex_count <= batch_max_vertices && index_count <= batch_max_indices);

        if (state != state_
            || vertices_.size() + vertex_count > batch_max_vertices
            || indices_.size() + index_count > batch_max_indices) {
            flush(context);
            state_ = state;
        }
        return static_cast<std::uint16_t>(vertices_.size());
    }

    auto Batch::flush(Context &context) noexcept -> void {
        if (indices_.empty()) {
            return;
        }

        vertex_buffer_.update(0, vertices_);
        index_buffer_.update(0, indices_);

        // Set on every draw: cheap, and a custom shader may be used elsewhere
        // in between. Raw GL, since Shader::set would call back to flush.
        const bool custom = state_.program != 0;
        const GlId program = custom ? state_.program : shader_.id();
        const std::int32_t u_projection = custom ? state_.u_projection : u_projection_;
        context.use_program(program);
        gl::detail::upload_mat4(u_projection, state_.transform);
        // The built-in shader's was set at creation.
        if (custom && state_.u_texture >= 0) {
            glUniform1i(state_.u_texture, 0);
        }

        ++context.frame.canvas_draws;
        context.submit({
            .target = state_.target,
            .program = program,
            .vertex_array = vertex_array_.id(),
            .index_type = gl::IndexType::uint16,
            .count = indices_.size(),
            .state = {.blend = state_.blend},
            .viewport = state_.viewport,
            .scissor = state_.clip,
            .textures = {state_.texture != 0 ? state_.texture : builtin_.id()},
        });

        vertices_.clear();
        indices_.clear();
    }

    auto Batch::uses_texture(GlId texture) const noexcept -> bool {
        return !indices_.empty() && (state_.texture == texture || state_.target.texture == texture);
    }

    auto Batch::uses_shader(GlId program) const noexcept -> bool {
        return !indices_.empty() && state_.program == program;
    }

    auto Batch::uses_target(GlId framebuffer) const noexcept -> bool {
        return !indices_.empty() && state_.target.framebuffer == framebuffer;
    }

    Batch::Batch(
        gl::Shader shader,
        std::int32_t u_projection,
        gl::Buffer<BatchVertex> vertex_buffer,
        gl::Buffer<std::uint16_t> index_buffer,
        gl::VertexArray vertex_array,
        Texture builtin
    )
        : shader_{std::move(shader)},
          u_projection_{u_projection},
          vertex_buffer_{std::move(vertex_buffer)},
          index_buffer_{std::move(index_buffer)},
          vertex_array_{std::move(vertex_array)},
          builtin_{std::move(builtin)} {
        // Never filled past this, so adding a shape never allocates.
        vertices_.reserve(batch_max_vertices);
        indices_.reserve(batch_max_indices);
    }
}
