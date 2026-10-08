// Buffers and vertex arrays: offsets and counts in elements, the index type
// taken from the buffer, dynamic updates.

#include "gpu.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <span>
#include <utility>

namespace {
    using Vertex = tgx_test::Cover::Vertex;

    // A vertex far outside, then the triangle over all of clip space.
    constexpr std::array vertices{Vertex{{9, 9}}, Vertex{{-1, -1}}, Vertex{{3, -1}}, Vertex{{-1, 3}}};

    [[nodiscard]] auto layout() -> tgx::gl::VertexArray {
        return tgx::gl::VertexArray::create<Vertex>(std::array{tgx::gl::VertexAttribute::of(0, &Vertex::position)});
    }

    // Whether a draw of the vertices fills the target.
    [[nodiscard]] auto fills(const tgx::gl::VertexArray &drawn) -> bool {
        const tgx::RenderTarget target = tgx_test::target({16, 16});
        const auto cover = tgx_test::Cover::create();
        auto &device = tgx_test::app().device();
        device.clear({.target = &target, .color = tgx::colors::black});
        device.draw(cover.shader, drawn, {.target = &target});
        const tgx::Image image = target.read();
        return image[1, 1] == tgx::colors::green && image[14, 14] == tgx::colors::green;
    }
}

TEST_CASE("a vertex buffer from an element on") {
    auto buffer = tgx::gl::Buffer<Vertex>::create(vertices);
    REQUIRE(buffer.has_value());
    CHECK(buffer->size() == 4);

    tgx::gl::VertexArray drawn = layout();
    drawn.set_vertex_buffer(*buffer, 1);
    CHECK(drawn.vertex_count() == 3);
    CHECK(fills(drawn));
}

TEST_CASE("32-bit indices, the type taken from the buffer") {
    auto buffer = tgx::gl::Buffer<Vertex>::create(vertices);
    REQUIRE(buffer.has_value());
    constexpr std::array<std::uint32_t, 3> indices{1, 2, 3};
    auto index_buffer = tgx::gl::Buffer<std::uint32_t>::create(indices);
    REQUIRE(index_buffer.has_value());

    tgx::gl::VertexArray drawn = layout();
    CHECK(drawn.index_count() == 0);
    drawn.set_vertex_buffer(*buffer);
    drawn.set_index_buffer(*index_buffer);
    CHECK(drawn.index_type() == tgx::gl::IndexType::uint32);
    CHECK(drawn.index_count() == 3);
    CHECK(fills(drawn));
}

TEST_CASE("a dynamic buffer updated from an element on") {
    auto buffer = tgx::gl::Buffer<Vertex>::create(4);
    REQUIRE(buffer.has_value());
    buffer->update(1, std::span{vertices}.subspan(1));

    tgx::gl::VertexArray drawn = layout();
    drawn.set_vertex_buffer(*buffer, 1);
    CHECK(fills(drawn));
}

TEST_CASE("a draw of first and count") {
    auto buffer = tgx::gl::Buffer<Vertex>::create(vertices);
    REQUIRE(buffer.has_value());

    tgx::gl::VertexArray drawn = layout();
    drawn.set_vertex_buffer(*buffer);
    const tgx::RenderTarget target = tgx_test::target({16, 16});
    const auto cover = tgx_test::Cover::create();
    auto &device = tgx_test::app().device();

    device.clear({.target = &target, .color = tgx::colors::black});
    device.draw(cover.shader, drawn, {.target = &target, .first = 1, .count = 3});
    CHECK(target.read()[8, 8] == tgx::colors::green);

    device.clear({.target = &target, .color = tgx::colors::black});
    device.draw(cover.shader, drawn, {.target = &target, .first = 1, .count = 0});
    CHECK(target.read()[8, 8] == tgx::colors::black);
}
