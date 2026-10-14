#pragma once

#include "tgx/core/error.hpp"

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

    // Settings for one Audio::play.
    //
    //     app->audio().play(jump, {.volume = 0.5f, .pan = -0.3f});
    struct PlayParams {
        // 1 as recorded, 0 silent. Not below 0.
        float volume{1.f};
        // -1 left, 0 center, 1 right. Within [-1, 1].
        float pan{0.f};
        // 1 as recorded, 2 an octave higher and twice as fast. Above 0.
        float pitch{1.f};
    };

    // A short sound kept whole in memory: a jump, a shot, a click. Can play
    // many times at once; each play runs to its end.
    //
    // Needs an Audio only to play. Destroying it, or moving another sound
    // into it, stops its plays; moving it keeps them playing. Load on any
    // thread, destroy on the thread that plays it.
    class Sound {
    public:
        // WAV, OGG Vorbis, MP3 or FLAC. Fails with Error::io if the file
        // cannot be read, Error::decode if it is not one of those formats.
        [[nodiscard]] static auto load(const std::filesystem::path &path) -> Result<Sound>;

        // Like load(), from file bytes in memory, e.g. an embedded asset.
        // Fails with Error::decode.
        [[nodiscard]] static auto decode(std::span<const std::byte> encoded) -> Result<Sound>;

        // A sound made in code. samples are in [-1, 1], interleaved by
        // channel (left, right, left, ...; up to 254 channels). sample_rate is
        // per channel. The samples are copied.
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

        // Stops its plays.
        auto release() noexcept -> void;

        std::vector<float> m_samples;
        int m_channels{0};
        int m_sample_rate{0};
        // Tags the voices playing this sound; 0 once moved from. A moved
        // vector keeps its buffer, so those voices play on.
        std::uint32_t m_id{0};
    };

    // Music or ambience, streamed from its file. Needs no per-frame calls.
    //
    //     auto music = tgx::Music::load("theme.ogg");
    //     music->set_looping(true);
    //     music->play();
    //
    // Load it after the Audio and destroy it before. With the Audio silent,
    // or no Audio, it loads and its getters work but it plays nothing, even
    // after an Audio is created later.
    class Music {
    public:
        // Stopped at the start. Formats and errors as in Sound::load. If the
        // stream cannot report its length (OGG Vorbis; any format on Windows
        // or with the Audio silent), the whole file is read once to measure it.
        [[nodiscard]] static auto load(const std::filesystem::path &path) -> Result<Music>;

        Music(const Music &) = delete;
        auto operator=(const Music &) -> Music & = delete;

        Music(Music &&) noexcept;
        auto operator=(Music &&) noexcept -> Music &;

        ~Music();

        // From the current position.
        auto play() noexcept -> void;

        // Keeps the position for play().
        auto pause() noexcept -> void;

        // Back to the start.
        auto stop() noexcept -> void;

        // False once it has ended, unless it loops.
        [[nodiscard]] auto playing() const noexcept -> bool;

        // Starts over after the end. Off by default.
        auto set_looping(bool looping) noexcept -> void;

        [[nodiscard]] auto looping() const noexcept -> bool;

        // 1 as recorded (default), 0 silent. Not below 0.
        auto set_volume(float volume) noexcept -> void;

        [[nodiscard]] auto volume() const noexcept -> float;

        // Playing or not. Past the end: the end, or wrapped around when
        // looping.
        auto seek(double seconds) noexcept -> void;

        // In seconds.
        [[nodiscard]] auto position() const noexcept -> double;

        // In seconds; 0 if the file does not report it.
        [[nodiscard]] auto duration() const noexcept -> double;

    private:
        explicit Music(std::unique_ptr<detail::MusicStream> stream) noexcept;

        std::unique_ptr<detail::MusicStream> m_stream;
    };

    // The sound output, one per process. App creates it: app->audio(). Its
    // state lives inside tgx, so it moves freely.
    //
    // Never fails. Without an output device (no sound card, headless) it
    // stays silent and logs it; everything else, loading included, works.
    class Audio {
    public:
        // The system's default output.
        [[nodiscard]] static auto create() noexcept -> Audio;

        Audio(const Audio &) = delete;
        auto operator=(const Audio &) -> Audio & = delete;

        Audio(Audio &&other) noexcept : m_owned{std::exchange(other.m_owned, false)} {
        }

        auto operator=(Audio &&other) noexcept -> Audio &;

        ~Audio();

        // False when silent.
        [[nodiscard]] auto active() const noexcept -> bool;

        // From the start, on top of whatever plays, the same sound included.
        // Up to 64 at once; past that, the oldest play is cut off.
        auto play(const Sound &sound, const PlayParams &params = {}) noexcept -> void;

        // Stops every play of the sound.
        auto stop(const Sound &sound) noexcept -> void;

        // Master volume for Sounds and Music. 1 as is, 0 silent. Not below 0.
        auto set_volume(float volume) noexcept -> void;

        [[nodiscard]] auto volume() const noexcept -> float;

    private:
        Audio() noexcept : m_owned{true} {
        }

        auto release() noexcept -> void;

        // False once moved from: another Audio closes the output.
        bool m_owned{false};
    };
}
