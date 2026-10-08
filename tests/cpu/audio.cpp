#include "tgx/core/audio.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <fstream>
#include <span>
#include <vector>

// Sounds and Music load without an Audio: nothing here opens an output.

namespace {
    // A WAV of 8 stereo frames at 8000 Hz, 16-bit.
    constexpr std::array<unsigned char, 76> wav{
        0x52, 0x49, 0x46, 0x46, 0x44, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45,
        0x66, 0x6d, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
        0x40, 0x1f, 0x00, 0x00, 0x00, 0x7d, 0x00, 0x00, 0x04, 0x00, 0x10, 0x00,
        0x64, 0x61, 0x74, 0x61, 0x20, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0xc0,
        0x00, 0x40, 0x00, 0xc0, 0x00, 0x40, 0x00, 0xc0, 0x00, 0x40, 0x00, 0xc0,
        0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f,
        0x00, 0x00, 0xff, 0x7f,
    };

    [[nodiscard]] auto wav_bytes() -> std::span<const std::byte> {
        return std::as_bytes(std::span{wav});
    }
}

TEST_CASE("Sound::from_samples") {
    const std::vector<float> samples(44100 * 2, 0.f);
    const tgx::Sound sound = tgx::Sound::from_samples(samples, 2, 44100);
    CHECK(sound.channels() == 2);
    CHECK(sound.sample_rate() == 44100);
    // Frames, not samples: two channels share a second's worth.
    CHECK(sound.duration() == doctest::Approx(1.0));
}

TEST_CASE("Sound::decode") {
    const auto sound = tgx::Sound::decode(wav_bytes());
    REQUIRE(sound.has_value());
    CHECK(sound->channels() == 2);
    CHECK(sound->sample_rate() == 8000);
    CHECK(sound->duration() == doctest::Approx(8.0 / 8000.0));

    const std::array garbage{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const auto bad = tgx::Sound::decode(garbage);
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error() == tgx::Error::decode);
}

TEST_CASE("Sound::load failures") {
    const auto missing = tgx::Sound::load("there/is/no/such/sound.ogg");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == tgx::Error::io);
}

TEST_CASE("a moved Sound keeps what it is") {
    tgx::Sound a = tgx::Sound::decode(wav_bytes()).value();
    const tgx::Sound b = std::move(a);
    CHECK(b.duration() == doctest::Approx(8.0 / 8000.0));
}

TEST_CASE("Music without an Audio loads, measures and stays quiet") {
    const tgx_test::TempFile file{"music.wav"};
    {
        std::ofstream out{file.path, std::ios::binary};
        out.write(reinterpret_cast<const char *>(wav.data()), static_cast<std::streamsize>(wav.size()));
    }

    auto music = tgx::Music::load(file.path);
    REQUIRE(music.has_value());
    CHECK(music->duration() == doctest::Approx(8.0 / 8000.0));
    music->play();
    CHECK_FALSE(music->playing());
    CHECK(music->position() == 0.0);

    // Settings hold even with nothing to play them on.
    CHECK_FALSE(music->looping());
    CHECK(music->volume() == 1.f);
    music->set_looping(true);
    music->set_volume(0.5f);
    CHECK(music->looping());
    CHECK(music->volume() == 0.5f);

    const auto missing = tgx::Music::load("there/is/no/such/music.ogg");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == tgx::Error::io);
}
