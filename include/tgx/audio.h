#pragma once

#include "tgx/error.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace tgx {
    class Audio;

    namespace detail {
        struct MusicStream;
    }

    // How one sound is played: Audio::play takes it.
    //
    //     app->audio().play(jump, {.volume = 0.5f, .pan = -0.3f});
    struct PlayParams {
        // 1 as recorded, 0 silent.
        float volume{1.f};
        // -1 all left, 0 in the middle, 1 all right.
        float pan{0.f};
        // 1 as recorded; 2 an octave higher, and twice as fast. Above 0.
        float pitch{1.f};
    };

    // A short sound held whole in memory, decoded once: a jump, a shot, a
    // click. Played with Audio::play, as many times at once as it is asked
    // to, each to its end.
    //
    // Needs no Audio to be made, only to be played. Destroying it, or moving
    // another into it, stops what of it is still playing; moving it keeps
    // it playing.
    class Sound {
    public:
        // Reads a WAV, OGG Vorbis, MP3 or FLAC file. Fails with Error::io
        // when the file cannot be read, Error::decode when it is not sound in
        // one of those formats.
        [[nodiscard]] static auto load(const std::filesystem::path &path) -> Result<Sound>;

        // The same from the bytes of such a file already in memory, e.g. an
        // embedded asset. Fails with Error::decode.
        [[nodiscard]] static auto decode(std::span<const std::byte> encoded) -> Result<Sound>;

        // A sound made in code: samples in [-1, 1], interleaved by channel
        // (left, right, left, ... for two; up to 254 channels), sample_rate
        // of them a second for each channel. Copied.
        //
        //     // a second of 440 Hz: samples[i] = std::sin(2 * pi * 440 * i / 44100)
        //     auto beep = tgx::Sound::from_samples(samples, 1, 44100);
        [[nodiscard]] static auto from_samples(
            std::span<const float> samples,
            int channels,
            int sample_rate
        ) -> Sound;

        Sound(const Sound &) = delete;
        auto operator=(const Sound &) -> Sound & = delete;

        Sound(Sound &&other) noexcept;
        auto operator=(Sound &&other) noexcept -> Sound &;

        ~Sound();

        [[nodiscard]] auto channels() const noexcept -> int { return m_channels; }

        [[nodiscard]] auto sample_rate() const noexcept -> int { return m_sample_rate; }

        // In seconds.
        [[nodiscard]] auto duration() const noexcept -> double;

    private:
        friend class Audio;

        Sound(std::vector<float> samples, int channels, int sample_rate) noexcept;

        // Stops what of it is playing.
        auto release() noexcept -> void;

        std::vector<float> m_samples;
        int m_channels{0};
        int m_sample_rate{0};
        // Which voices are its, 0 once moved from. A moved vector keeps its
        // buffer, so voices of it play on from the same samples.
        std::uint32_t m_id{0};
    };

    // A long piece, music or ambience, read from its file a little at a time
    // while it plays rather than held whole. Controls itself; nothing needs
    // calling every frame.
    //
    //     auto music = tgx::Music::load("theme.ogg");
    //     music->set_looping(true);
    //     music->play();
    //
    // Lives inside the Audio: loaded after it, destroyed before it. With the
    // Audio silent, or with none at all, it loads and answers but plays
    // nothing, and stays so even once an Audio is made.
    class Music {
    public:
        // Opens a WAV, OGG Vorbis, MP3 or FLAC file, stopped at its start.
        // Fails with Error::io when the file cannot be read, Error::decode
        // when it is not sound in one of those formats.
        [[nodiscard]] static auto load(const std::filesystem::path &path) -> Result<Music>;

        Music(const Music &) = delete;
        auto operator=(const Music &) -> Music & = delete;

        Music(Music &&) noexcept;
        auto operator=(Music &&) noexcept -> Music &;

        ~Music();

        // Plays from where it is: its start, or where pause() left it.
        auto play() noexcept -> void;

        // Stops where it is, to go on from there.
        auto pause() noexcept -> void;

        // Stops and goes back to the start.
        auto stop() noexcept -> void;

        // Whether it is playing now; false once it has ended, unless looping.
        [[nodiscard]] auto playing() const noexcept -> bool;

        // Starts over at its end; off by default.
        auto set_looping(bool looping) noexcept -> void;

        // 1 as recorded, 0 silent.
        auto set_volume(float volume) noexcept -> void;

        // Moves to the second, playing or not.
        auto seek(double seconds) noexcept -> void;

        // In seconds from its start.
        [[nodiscard]] auto position() const noexcept -> double;

        // In seconds; 0 if the file does not tell.
        [[nodiscard]] auto length() const noexcept -> double;

    private:
        explicit Music(std::unique_ptr<detail::MusicStream> stream) noexcept;

        std::unique_ptr<detail::MusicStream> m_stream;
    };

    // The sound output, one per process, as the Window is the one window. App
    // makes it; app->audio() reaches it. Like the Device, it only owns the
    // output and its state is kept in one place inside tgx, so it can be moved
    // freely.
    //
    // Never fails: without an output device (no sound card, a headless
    // machine) it stays silent, says so in the log, and everything else works
    // as before, Sounds and Music loading included. A game runs the same
    // without sound.
    class Audio {
    public:
        // Opens the system's default output.
        [[nodiscard]] static auto create() noexcept -> Audio;

        Audio(const Audio &) = delete;
        auto operator=(const Audio &) -> Audio & = delete;

        Audio(Audio &&other) noexcept : m_owned{std::exchange(other.m_owned, false)} {
        }

        auto operator=(Audio &&other) noexcept -> Audio &;

        ~Audio();

        // Whether there is an output to hear: false when it stays silent.
        [[nodiscard]] auto active() const noexcept -> bool;

        // Starts the sound from its beginning, on top of whatever plays,
        // itself included. Up to 64 sounds play at once; past that the one
        // playing longest is cut off for it.
        auto play(const Sound &sound, const PlayParams &params = {}) noexcept -> void;

        // Cuts off every play of the sound still going.
        auto stop(const Sound &sound) noexcept -> void;

        // Of everything, Sounds and Music alike: 1 as they are, 0 silent.
        auto set_volume(float volume) noexcept -> void;

        [[nodiscard]] auto volume() const noexcept -> float;

    private:
        Audio() noexcept : m_owned{true} {
        }

        auto release() noexcept -> void;

        // False once moved from: the output is someone else's to close.
        bool m_owned{false};
    };
}
