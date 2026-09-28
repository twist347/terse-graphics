#pragma once

#include "tgx/error.h"
#include "tgx/gl/handle.h"

#include <string>
#include <string_view>
#include <utility>

namespace tgx {
    class Device;
}

namespace tgx::gl {
    namespace detail {
        auto delete_program(GlId id) noexcept -> void;
    }

    // A linked GPU program: a vertex and a fragment stage. Sources carry their
    // own #version line.
    //
    // The Device in from_source() is proof that GL functions are loaded; it is
    // not stored.
    class Shader {
    public:
        // Fails with Error::compile or Error::link. The driver's log is written to
        // out_log either way: on success it may still hold warnings.
        [[nodiscard]] static auto from_source(
            Device &device,
            std::string_view vertex,
            std::string_view fragment,
            std::string *out_log = nullptr
        ) -> Result<Shader>;

        Shader(const Shader &) = delete;
        auto operator=(const Shader &) -> Shader & = delete;

        Shader(Shader &&) noexcept = default;
        auto operator=(Shader &&) noexcept -> Shader & = default;

        [[nodiscard]] auto id() const noexcept -> GlId;

    private:
        explicit Shader(Handle<detail::delete_program> handle) noexcept : m_handle{std::move(handle)} {
        }

        Handle<detail::delete_program> m_handle;
    };
}
