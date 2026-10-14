#pragma once

#include "tgx/core/color.hpp"
#include "tgx/core/error.hpp"
#include "tgx/core/handle.hpp"
#include "tgx/core/math.hpp"

#include "tgx/gl/texture_slot.hpp"
#include "tgx/gl/version.hpp"

#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tgx::gl {
    class Shader;

    namespace detail {
        auto delete_program(GlId id) noexcept -> void;

        // How GL reads a vertex input or attribute: as floats (plain or
        // normalized), signed or unsigned integers. A mismatch reads garbage.
        enum class ComponentKind : std::int32_t {
            floating,
            sint,
            uint,
        };

        // An active uniform. An array is listed once, without "[0]".
        struct ShaderUniform {
            std::string name;
            std::int32_t location{-1};
            // E.g. GL_FLOAT_VEC4.
            std::uint32_t gl_type{0};
            // 1 unless it is an array.
            std::int32_t count{1};
        };

        // An active input of the vertex stage.
        struct VertexInput {
            std::string name;
            std::uint32_t location{0};
            // Consecutive locations used. A mat4 uses four.
            std::uint32_t slots{1};
            ComponentKind kind{ComponentKind::floating};
        };

        // An active sampler2D uniform and its texture slot.
        struct ShaderSampler {
            std::string name;
            std::int32_t location{-1};
            // Last set with a TextureSlot. GL starts every sampler at 0.
            std::uint32_t slot{0};
        };

        // For Device::draw to check against the vertex array. Empty without
        // asserts.
        [[nodiscard]] auto vertex_inputs(const Shader &shader) noexcept -> std::span<const VertexInput>;

        // For Device::draw to check that every slot read has a texture.
        [[nodiscard]] auto samplers(const Shader &shader) noexcept -> std::span<const ShaderSampler>;
    }

    // Color is sent as a vec4 in [0, 1]. A bool uniform takes an int or a
    // uint. A sampler2D takes only a TextureSlot.
    template<typename T>
    concept UniformValue = std::same_as<T, float>
                           || std::same_as<T, std::int32_t>
                           || std::same_as<T, std::uint32_t>
                           || std::same_as<T, Vec2>
                           || std::same_as<T, Vec3>
                           || std::same_as<T, Vec4>
                           || std::same_as<T, Mat4>
                           || std::same_as<T, Color>
                           || std::same_as<T, TextureSlot>;

    template<UniformValue T>
    class Uniform;

    namespace detail {
        // -1 if there is none. For tgx's own draws, which write to GL
        // directly instead of through Shader::set.
        template<UniformValue T>
        [[nodiscard]] constexpr auto location(Uniform<T> uniform) noexcept -> std::int32_t;
    }

    // A uniform of one Shader, looked up by name once. A default-constructed
    // one refers to nothing.
    template<UniformValue T>
    class Uniform {
    public:
        Uniform() noexcept = default;

    private:
        friend class Shader;

        template<UniformValue U>
        friend constexpr auto detail::location(Uniform<U> uniform) noexcept -> std::int32_t;

        Uniform(GlId program, std::int32_t location) noexcept : m_program{program}, m_location{location} {
        }

        // To catch setting it on another Shader.
        GlId m_program{0};
        // -1 when not found. Setting it then does nothing.
        std::int32_t m_location{-1};
    };

    template<UniformValue T>
    constexpr auto detail::location(Uniform<T> uniform) noexcept -> std::int32_t {
        return uniform.m_location;
    }

    // A linked vertex and fragment program. Each source has its own #version
    // line; TGX_GLSL_VERSION (tgx/gl/version.hpp) matches the context.
    //
    // Create it after the Device and destroy it before the Device. Setting a
    // uniform or destroying it first draws the batched Canvas shapes that use
    // it.
    class Shader {
    public:
        // Fails with Error::compile or Error::link. The driver's log always
        // goes to out_log; on success it may hold warnings.
        [[nodiscard]] static auto from_source(
            std::string_view vertex,
            std::string_view fragment,
            std::string *out_log = nullptr
        ) -> Result<Shader>;

        Shader(const Shader &) = delete;
        auto operator=(const Shader &) -> Shader & = delete;

        Shader(Shader &&) noexcept = default;
        auto operator=(Shader &&) noexcept -> Shader & = default;

        // Asserts that it is active, matches T and is not an array (not
        // supported yet). Without asserts, a missing one sets nothing.
        //
        //     const auto u_mvp = shader.uniform<Mat4>("u_mvp");
        template<UniformValue T>
        [[nodiscard]] auto uniform(std::string_view name) const noexcept -> Uniform<T>;

        // Makes the program current: GL 3.3 sets uniforms only on the current
        // program. The value converts to T, so an int literal sets a float.
        template<UniformValue T>
        auto set(Uniform<T> uniform, const std::type_identity_t<T> &value) noexcept -> void;

        [[nodiscard]] auto id() const noexcept -> GlId { return m_handle.get(); }

    private:
        friend auto detail::vertex_inputs(const Shader &shader) noexcept -> std::span<const detail::VertexInput>;
        friend auto detail::samplers(const Shader &shader) noexcept -> std::span<const detail::ShaderSampler>;

        Shader(
            tgx::detail::Handle<detail::delete_program> handle,
            std::vector<detail::ShaderUniform> uniforms,
            std::vector<detail::VertexInput> inputs,
            std::vector<detail::ShaderSampler> samplers
        ) noexcept
            : m_handle{std::move(handle)},
              m_uniforms{std::move(uniforms)},
              m_inputs{std::move(inputs)},
              m_samplers{std::move(samplers)} {
        }

        tgx::detail::Handle<detail::delete_program> m_handle;
        std::vector<detail::ShaderUniform> m_uniforms;
        std::vector<detail::VertexInput> m_inputs;
        std::vector<detail::ShaderSampler> m_samplers;
    };
}
