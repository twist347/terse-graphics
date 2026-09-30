#pragma once

#include "tgx/color.h"
#include "tgx/error.h"
#include "tgx/math.h"

#include "tgx/gl/handle.h"
#include "tgx/gl/version.h"

#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tgx {
    class Device;
}

namespace tgx::gl {
    class Shader;

    namespace detail {
        auto delete_program(GlId id) noexcept -> void;

        // What a vertex input or attribute holds, as far as GL's reading of the
        // buffer is concerned: floats (plain or normalized), signed or unsigned
        // integers. Mixing them reads garbage.
        enum class ComponentKind : std::int32_t {
            floating,
            sint,
            uint,
        };

        // An active uniform, as the linker reported it. Arrays are listed once,
        // by name without the "[0]".
        struct ShaderUniform {
            std::string name;
            std::int32_t location{-1};
            // The GL type enum, e.g. GL_FLOAT_VEC4.
            std::uint32_t gl_type{0};
            // Elements, 1 unless it is an array.
            std::int32_t count{1};
        };

        // An active input of the vertex stage.
        struct VertexInput {
            std::string name;
            std::uint32_t location{0};
            // Consecutive locations taken: a mat4 input takes four.
            std::uint32_t slots{1};
            ComponentKind kind{ComponentKind::floating};
        };

        // For Device::draw, which checks them against the vertex array. Empty in
        // builds without asserts: nothing else reads them.
        [[nodiscard]] auto vertex_inputs(const Shader &shader) noexcept -> std::span<const VertexInput>;
    }

    // What a uniform can be set with. Color goes as a vec4 with channels in
    // [0, 1]; a bool uniform takes an int or a uint.
    template<typename T>
    concept UniformValue = std::same_as<T, float>
                           || std::same_as<T, std::int32_t>
                           || std::same_as<T, std::uint32_t>
                           || std::same_as<T, Vec2>
                           || std::same_as<T, Vec3>
                           || std::same_as<T, Vec4>
                           || std::same_as<T, Mat4>
                           || std::same_as<T, Color>;

    // A uniform of one Shader, looked up by name once and then set with values
    // of T. A default-constructed one refers to nothing.
    template<UniformValue T>
    class Uniform {
    public:
        Uniform() noexcept = default;

    private:
        friend class Shader;

        Uniform(GlId program, std::int32_t location) noexcept : m_program{program}, m_location{location} {
        }

        // Which Shader it came from, to catch it being set on another one.
        GlId m_program{0};
        // -1 when the uniform was not found: setting it does nothing.
        std::int32_t m_location{-1};
    };

    // A linked GPU program: a vertex and a fragment stage. Sources carry their
    // own #version line; TGX_GLSL_VERSION (tgx/gl/version.h) is the one that
    // matches the context.
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

        // Looks a uniform up by name, for setting it later without the name.
        // Asserts that it is active, of a GLSL type T fits and not an array
        // (arrays are not supported yet); without asserts a missing one gives a
        // Uniform that sets nothing.
        //
        //     const auto u_mvp = shader.uniform<Mat4>("u_mvp");
        template<UniformValue T>
        [[nodiscard]] auto uniform(std::string_view name) const noexcept -> Uniform<T>;

        // Makes the program current, as GL 3.3 can only set uniforms of the
        // current one, and writes the value. The value converts to T, so an int
        // literal sets a float uniform.
        template<UniformValue T>
        auto set(Uniform<T> uniform, const std::type_identity_t<T> &value) noexcept -> void;

        [[nodiscard]] auto id() const noexcept -> GlId;

    private:
        friend auto detail::vertex_inputs(const Shader &shader) noexcept -> std::span<const detail::VertexInput>;

        Shader(
            Handle<detail::delete_program> handle,
            std::vector<detail::ShaderUniform> uniforms,
            std::vector<detail::VertexInput> inputs
        ) noexcept
            : m_handle{std::move(handle)},
              m_uniforms{std::move(uniforms)},
              m_inputs{std::move(inputs)} {
        }

        Handle<detail::delete_program> m_handle;
        std::vector<detail::ShaderUniform> m_uniforms;
        std::vector<detail::VertexInput> m_inputs;
    };
}
