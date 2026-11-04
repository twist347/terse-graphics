#include "gpu/batch_gpu.hpp"

#include "tgx/core/assert.hpp"
#include "tgx/core/image.hpp"

#include "tgx/gl/draw.hpp"
#include "tgx/gl/version.hpp"

#include "canvas/font.hpp"
#include "gpu/canvas_shader.hpp"
#include "gpu/gl/shader_internal.hpp"

#include <glad/gl.h>

#include <cstdint>
#include <limits>
#include <utility>

namespace {
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

    // Every vertex is reachable by a uint16 index.
    static_assert(tgx::detail::batch_max_vertices - 1 <= std::numeric_limits<std::uint16_t>::max());
}

namespace tgx::detail {
    auto BatchGpu::create() -> Result<BatchGpu> {
        auto shader = gl::Shader::from_source(vertex_source, fragment_source);
        if (!shader) {
            // Tested on every supported platform; a driver rejecting it is
            // the platform's fault.
            TGX_ASSERT_MSG(false, "the 2D batch shader failed: {}", shader.error());
            return std::unexpected{Error::platform};
        }
        const auto bindings = canvas_shader_bindings(&*shader);
        shader->set(shader->uniform<gl::TextureSlot>("u_texture"), {0});

        auto vertex_buffer = gl::Buffer<BatchVertex>::create(batch_max_vertices);
        if (!vertex_buffer) {
            return std::unexpected{vertex_buffer.error()};
        }
        auto index_buffer = gl::Buffer<std::uint16_t>::create(batch_max_indices);
        if (!index_buffer) {
            return std::unexpected{index_buffer.error()};
        }

        const auto layout = canvas_vertex_layout();
        auto vertex_array = gl::VertexArray::create<BatchVertex>(layout);
        if (!vertex_array) {
            return std::unexpected{vertex_array.error()};
        }
        vertex_array->set_vertex_buffer(*vertex_buffer);
        vertex_array->set_index_buffer(*index_buffer);

        auto canvas_atlas = Texture::create(make_default_font_atlas(), {.filter = TextureFilter::nearest});
        if (!canvas_atlas) {
            return std::unexpected{canvas_atlas.error()};
        }

        return BatchGpu{
            std::move(*shader),
            bindings.u_projection,
            std::move(*vertex_buffer),
            std::move(*index_buffer),
            std::move(*vertex_array),
            std::move(*canvas_atlas),
        };
    }

    auto BatchGpu::prepare_draw(const Batch &batch, GlState &gl_state) noexcept -> DrawCall {
        TGX_ASSERT(!batch.empty());
        vertex_buffer_.update(0, batch.vertices());
        index_buffer_.update(0, batch.indices());
        gl_state.count_canvas_upload(
            batch.vertices().size(), batch.indices().size(),
            batch.vertices().size_bytes() + batch.indices().size_bytes()
        );

        // Set on every draw: a custom shader may be used elsewhere in between.
        // Raw GL, since Shader::set would call back into Context to flush.
        const BatchState &state = batch.state();
        const bool custom = state.program != 0;
        const GlId program = custom ? state.program : shader_.id();
        const std::int32_t u_projection = custom ? state.u_projection : u_projection_;
        gl_state.use_program(program);
        gl::detail::upload_mat4(u_projection, state.transform);
        // The built-in shader's was set at creation.
        if (custom && state.u_texture >= 0) {
            glUniform1i(state.u_texture, 0);
        }

        return {
            .target = state.target,
            .program = program,
            .vertex_array = vertex_array_.id(),
            .index_type = gl::IndexType::uint16,
            .count = batch.indices().size(),
            .state = {.blend = state.blend},
            .viewport = state.viewport,
            .scissor = state.clip,
            .textures = {state.texture != 0 ? state.texture : canvas_atlas_.id()},
        };
    }

    BatchGpu::BatchGpu(
        gl::Shader shader,
        std::int32_t u_projection,
        gl::Buffer<BatchVertex> vertex_buffer,
        gl::Buffer<std::uint16_t> index_buffer,
        gl::VertexArray vertex_array,
        Texture canvas_atlas
    )
        : shader_{std::move(shader)},
          u_projection_{u_projection},
          vertex_buffer_{std::move(vertex_buffer)},
          index_buffer_{std::move(index_buffer)},
          vertex_array_{std::move(vertex_array)},
          canvas_atlas_{std::move(canvas_atlas)} {
    }
}
