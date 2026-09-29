#pragma once

#include <cstdint>
#include <expected>

namespace tgx {
    enum class Error : std::int32_t {
        io,
        decode,
        compile,
        link,
        out_of_mem,
        unsupported,
        platform,
        invalid_argument,
    };

    [[nodiscard]] constexpr auto to_str(Error err) noexcept -> const char * {
        switch (err) {
            case Error::io: return "io";
            case Error::decode: return "decode";
            case Error::compile: return "compile";
            case Error::link: return "link";
            case Error::out_of_mem: return "out of memory";
            case Error::unsupported: return "unsupported";
            case Error::platform: return "platform";
            case Error::invalid_argument: return "invalid argument";
        }
        return "unknown";
    }

    template<typename T>
    using Result = std::expected<T, Error>;
}
