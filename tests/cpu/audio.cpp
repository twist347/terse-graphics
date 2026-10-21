#include "tgx/core/audio.hpp"

#include "core/tone.hpp"

#include "support.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <thread>
#include <vector>

// Sounds and Music load without an Audio. Only the Playback test opens an
// output, and checks plays only when there is a device.

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
    const auto sound = tgx::Sound::from_samples(samples, 2, 44100);
    CHECK(sound.channels() == 2);
    CHECK(sound.sample_rate() == 44100);
    // Frames, not samples: two channels share a second's worth.
    CHECK(sound.duration() == doctest::Approx(1.0));
}

TEST_CASE("Sound::tone") {
    const auto sound = tgx::Sound::tone({.duration = 0.5f});
    CHECK(sound.channels() == 1);
    CHECK(sound.sample_rate() == tgx::detail::tone_rate);
    CHECK(sound.duration() == doctest::Approx(0.5).epsilon(1e-4));
}

TEST_CASE("tone waves start at 0 and rise, within [-1, 1]") {
    using tgx::Waveform;
    using tgx::detail::wave_at;
    for (const Waveform waveform : {Waveform::sine, Waveform::triangle, Waveform::sawtooth}) {
        CAPTURE(static_cast<int>(waveform));
        CHECK(wave_at(waveform, 0) == doctest::Approx(0));
        CHECK(wave_at(waveform, 0.1) > 0);
        for (double phase = 0; phase < 1; phase += 0.01) {
            CHECK(std::abs(wave_at(waveform, phase)) <= 1.0 + 1e-9);
        }
    }
    CHECK(wave_at(Waveform::triangle, 0.25) == doctest::Approx(1));
    CHECK(wave_at(Waveform::triangle, 0.75) == doctest::Approx(-1));
    CHECK(wave_at(Waveform::sawtooth, 0.5) == doctest::Approx(-1));
    CHECK(wave_at(Waveform::square, 0.25) == 1);
    CHECK(wave_at(Waveform::square, 0.75) == -1);
}

TEST_CASE("a tone starts and ends silent, peaks at its volume") {
    // A square has no zero crossings to hide a click: only the envelope can.
    const auto samples = tgx::detail::tone_samples({
        .waveform = tgx::Waveform::square, .frequency = 100, .duration = 0.1f, .volume = 0.4f,
    });
    CHECK(samples.front() == 0.f);
    CHECK(samples.back() == 0.f);
    const auto peak = std::ranges::max(samples, {}, [](float s) { return std::abs(s); });
    CHECK(std::abs(peak) == doctest::Approx(0.4f));
}

TEST_CASE("a tone with no attack starts at its peak") {
    const auto samples = tgx::detail::tone_samples({.waveform = tgx::Waveform::square, .volume = 0.3f, .attack = 0});
    CHECK(samples.front() == doctest::Approx(0.3f));
}

TEST_CASE("a decaying tone dies away") {
    const auto samples = tgx::detail::tone_samples({.duration = 1, .volume = 1, .decay = 6});
    const auto loudest = [&](std::size_t from) {
        float most = 0;
        for (std::size_t i = from; i < from + 1000; ++i) {
            most = std::max(most, std::abs(samples[i]));
        }
        return most;
    };
    // e^(-6 * 0.5): a twentieth at half a second.
    CHECK(loudest(22050) == doctest::Approx(std::exp(-3.f)).epsilon(0.05));
    CHECK(loudest(1000) > loudest(22050));
}

TEST_CASE("tone noise is the same every time and follows its frequency") {
    constexpr tgx::ToneParams params{.waveform = tgx::Waveform::noise, .frequency = 100, .attack = 0};
    const auto a = tgx::detail::tone_samples(params);
    const auto b = tgx::detail::tone_samples(params);
    CHECK(a == b);
    // 100 Hz: a level held for 1 / 200 s, 220 samples and a half.
    CHECK(a[0] == a[219]);
    CHECK(a[0] != a[221]);
}

TEST_CASE("Sound::decode") {
    const auto sound = tgx::Sound::decode(wav_bytes());
    REQUIRE(sound.has_value());
    CHECK(sound->channels() == 2);
    CHECK(sound->sample_rate() == 8000);
    CHECK(sound->duration() == doctest::Approx(8.0 / 8000.0));

    constexpr std::array garbage{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const auto bad = tgx::Sound::decode(garbage);
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error() == tgx::Error::decode);
}

TEST_CASE("Sound::decode of a header claiming terabytes fails without reserving them") {
    // FLAC: a STREAMINFO of 8 channels and 2^36 - 1 frames, then junk.
    std::vector<std::byte> flac{};
    const auto put = [&](unsigned value) { flac.push_back(static_cast<std::byte>(value)); };
    for (const char c: {'f', 'L', 'a', 'C'}) {
        put(static_cast<unsigned char>(c));
    }
    for (const unsigned value: {0x80u, 0u, 0u, 34u, 0x10u, 0u, 0x10u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}) {
        put(value);
    }
    constexpr std::uint64_t info = 44100ull << 44 | 7ull << 41 | 15ull << 36 | 0xfffffffffull;
    for (int shift = 56; shift >= 0; shift -= 8) {
        put(static_cast<unsigned>(info >> shift) & 0xffu);
    }
    flac.resize(flac.size() + 16);   // the MD5
    flac.resize(flac.size() + 64, std::byte{0xff});

    const auto sound = tgx::Sound::decode(flac);
    REQUIRE_FALSE(sound.has_value());
    CHECK(sound.error() == tgx::Error::decode);
}

TEST_CASE("Sound::load failures") {
    const auto missing = tgx::Sound::load("there/is/no/such/sound.ogg");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == tgx::Error::io);
}

TEST_CASE("a moved Sound keeps what it is") {
    auto a = tgx::Sound::decode(wav_bytes()).value();
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

TEST_CASE("a Playback stops or pauses its own play") {
    tgx::Playback none{};
    none.pause();
    none.resume();
    none.stop();
    CHECK_FALSE(none.playing());

    auto audio = tgx::Audio::create();
    if (!audio.active()) {
        MESSAGE("no output device: plays not checked");
        return;
    }

    // A second of silence: long enough to stay playing through the checks.
    const std::vector<float> silence(44100, 0.f);
    const auto sound = tgx::Sound::from_samples(silence, 1, 44100);

    tgx::Playback a = audio.play(sound);
    const tgx::Playback b = audio.play(sound);
    CHECK(a.playing());

    a.pause();
    CHECK_FALSE(a.playing());
    CHECK(b.playing());
    a.resume();
    CHECK(a.playing());

    a.stop();
    CHECK_FALSE(a.playing());
    CHECK(b.playing());
    a.resume();
    CHECK_FALSE(a.playing());

    // The voice a held is free again; a new play there is not a.
    const tgx::Playback c = audio.play(sound);
    CHECK(c.playing());
    CHECK_FALSE(a.playing());

    audio.stop(sound);
    CHECK_FALSE(b.playing());
    CHECK_FALSE(c.playing());

    // Ended: resume() does not start it over.
    const std::vector<float> blip(441, 0.f);
    const auto short_sound = tgx::Sound::from_samples(blip, 1, 44100);
    tgx::Playback ended = audio.play(short_sound);
    std::this_thread::sleep_for(std::chrono::milliseconds{300});
    CHECK_FALSE(ended.playing());
    ended.resume();
    CHECK_FALSE(ended.playing());
}
