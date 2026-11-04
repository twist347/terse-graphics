#pragma once

#include "tgx/core/error.hpp"
#include "tgx/core/texture.hpp"

#include "tgx/gl/buffer.hpp"
#include "tgx/gl/shader.hpp"
#include "tgx/gl/vertex_array.hpp"

#include "gpu/batch.hpp"
#include "gpu/gl_state.hpp"

#include <cstdint>

namespace tgx::detail {
    // GPU storage, shader and atlas for Canvas batches. Uploads the geometry
    // and uniforms; Context submits the resulting draw and clears the batch.
    class BatchGpu {
    public:
        [[nodiscard]] static auto create() -> Result<BatchGpu>;

        BatchGpu(const BatchGpu &) = delete;
        auto operator=(const BatchGpu &) -> BatchGpu & = delete;
        BatchGpu(BatchGpu &&) noexcept = default;
        auto operator=(BatchGpu &&) noexcept -> BatchGpu & = default;

        // Uploads geometry, makes the program current and sets its uniforms.
        // Execute the returned draw immediately. Requires a nonempty batch;
        // its CPU geometry stays unchanged.
        [[nodiscard]] auto prepare_draw(const Batch &batch, GlState &gl_state) noexcept -> DrawCall;

    private:
        BatchGpu(
            gl::Shader shader,
            std::int32_t u_projection,
            gl::Buffer<BatchVertex> vertex_buffer,
            gl::Buffer<std::uint16_t> index_buffer,
            gl::VertexArray vertex_array,
            Texture canvas_atlas
        );

        gl::Shader shader_;
        std::int32_t u_projection_{};
        gl::Buffer<BatchVertex> vertex_buffer_;
        gl::Buffer<std::uint16_t> index_buffer_;
        gl::VertexArray vertex_array_;
        // The default font and a white block that shapes sample, so text
        // and shapes in a row share one shader and one draw.
        Texture canvas_atlas_;
    };
}
