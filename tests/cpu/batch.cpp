// Batching decisions need neither a window nor GPU resources.

#include "gpu/batch.hpp"

#include <doctest/doctest.h>

TEST_CASE("Batch accepts the exact remaining vertex and index capacities") {
    using namespace tgx::detail;
    Batch batch;
    const BatchState state{};
    CHECK(batch.can_append(state, batch_max_vertices, batch_max_indices));
    CHECK_FALSE(batch.can_append(state, batch_max_vertices + 1, 3));
    CHECK_FALSE(batch.can_append(state, 3, batch_max_indices + 1));

    REQUIRE(batch.start(state) == 0);
    batch.push_vertex({{0, 0}, {}, tgx::colors::red});
    batch.push_vertex({{1, 0}, {}, tgx::colors::red});
    batch.push_vertex({{0, 1}, {}, tgx::colors::red});
    batch.push_index(0);
    batch.push_index(1);
    batch.push_index(2);

    CHECK(batch.can_append(state, batch_max_vertices - 3, batch_max_indices - 3));
    CHECK_FALSE(batch.can_append(state, batch_max_vertices - 2, 3));
    CHECK_FALSE(batch.can_append(state, 3, batch_max_indices - 2));
    CHECK(batch.start(state) == 3);

    batch.clear();
    CHECK(batch.can_append(state, batch_max_vertices, batch_max_indices));
    CHECK(batch.start(state) == 0);
}

TEST_CASE("Batch tracks resource uses only while geometry is waiting") {
    tgx::detail::Batch batch;
    const tgx::detail::BatchState state{
        .target = {1, 2, {4, 4}, false},
        .texture = 3,
        .program = 4,
    };
    CHECK_FALSE(batch.uses_texture(3));
    CHECK_FALSE(batch.uses_shader(4));
    CHECK_FALSE(batch.uses_target(1));

    REQUIRE(batch.start(state) == 0);
    batch.push_vertex({});
    batch.push_vertex({});
    batch.push_vertex({});
    batch.push_index(0);
    batch.push_index(1);
    batch.push_index(2);
    CHECK(batch.uses_texture(2)); // Also a write to the target's texture.
    CHECK(batch.uses_texture(3));
    CHECK(batch.uses_shader(4));
    CHECK(batch.uses_target(1));
    CHECK_FALSE(batch.uses_texture(5));
    CHECK_FALSE(batch.uses_shader(5));
    CHECK_FALSE(batch.uses_target(5));

    auto next = state;
    next.program = 5;
    CHECK_FALSE(batch.can_append(next, 3, 3));
    batch.clear();
    CHECK_FALSE(batch.uses_texture(2));
    CHECK_FALSE(batch.uses_texture(3));
    CHECK_FALSE(batch.uses_shader(4));
    CHECK_FALSE(batch.uses_target(1));
    CHECK(batch.can_append(next, 3, 3));
    CHECK(batch.start(next) == 0);
}
