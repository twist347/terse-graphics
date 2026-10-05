#include "tgx/audio.h"

#include "tgx/assert.h"

#include "file.h"
#include "log_internal.h"

#include <miniaudio/miniaudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

namespace {
    // Plays of Sounds at once. Each holds a sound of miniaudio's, which may
    // not move while it plays, so they live in a fixed pool.
    constexpr std::size_t max_voices = 64;

    // One play of a Sound: miniaudio reads its samples through data, in
    // place, and mixes them through sound.
    struct Voice {
        ma_audio_buffer_ref data;
        ma_sound sound;
        // The Sound's id, 0 while free.
        std::uint32_t owner{0};
        // When it started, in plays: the oldest is cut off first.
        std::uint64_t started{0};
    };

    // The one output, while an Audio has it open. Allocated, as neither the
    // engine nor the voices may move.
    struct AudioState {
        ma_engine engine;
        std::array<Voice, max_voices> voices;
        std::uint64_t plays{0};
    };

    // Null when there is no Audio or it stays silent.
    std::unique_ptr<AudioState> s_audio;
    // As last set, kept while silent too, so volume() answers the same.
    float s_volume{1.f};
    // Sounds may be loaded on other threads, as assets often are.
    std::atomic<std::uint32_t> s_next_sound_id{1};

    auto free_voice(Voice &voice) noexcept -> void {
        ma_sound_uninit(&voice.sound);
        ma_audio_buffer_ref_uninit(&voice.data);
        voice.owner = 0;
    }

    // Frees the voices that have played to their end.
    auto reclaim_voices(AudioState &audio) noexcept -> void {
        for (Voice &voice: audio.voices) {
            if (voice.owner != 0 && ma_sound_at_end(&voice.sound)) {
                free_voice(voice);
            }
        }
    }

    // A free voice, or the one that has played longest, cut off.
    [[nodiscard]] auto take_voice(AudioState &audio) noexcept -> Voice & {
        reclaim_voices(audio);
        const auto free = std::ranges::find(audio.voices, 0u, &Voice::owner);
        if (free != audio.voices.end()) {
            return *free;
        }
        Voice &oldest = *std::ranges::min_element(audio.voices, {}, &Voice::started);
        free_voice(oldest);
        return oldest;
    }

    auto stop_voices(std::uint32_t owner) noexcept -> void {
        if (!s_audio || owner == 0) {
            return;
        }
        for (Voice &voice: s_audio->voices) {
            if (voice.owner == owner) {
                free_voice(voice);
            }
        }
    }
}

namespace tgx::detail {
    // Music's miniaudio side. live is false while the Audio stays silent:
    // there is no sound to drive then, only what the file told.
    struct MusicStream {
        ma_sound sound;
        bool live{false};
        double length{0.0};

        MusicStream() noexcept = default;
        MusicStream(const MusicStream &) = delete;
        auto operator=(const MusicStream &) -> MusicStream & = delete;

        ~MusicStream() {
            if (live) {
                ma_sound_uninit(&sound);
            }
        }
    };
}

namespace {
    // All of it, as 32-bit floats at the rate and channels it has. A file
    // with no samples in it is no sound either.
    [[nodiscard]] auto decode_all(std::span<const std::byte> encoded) -> tgx::Result<tgx::Sound> {
        // 32-bit floats, and otherwise as the sound was recorded: its own
        // rate and channels, which the engine converts as it plays.
        const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        ma_decoder decoder;
        if (ma_decoder_init_memory(encoded.data(), encoded.size(), &config, &decoder) != MA_SUCCESS) {
            return std::unexpected{tgx::Error::decode};
        }

        ma_uint32 channels = 0;
        ma_uint32 rate = 0;
        ma_decoder_get_data_format(&decoder, nullptr, &channels, &rate, nullptr, 0);

        // In chunks: some formats (Vorbis) cannot tell their length up front.
        std::vector<float> samples;
        constexpr ma_uint64 chunk = 4096;
        for (;;) {
            const std::size_t at = samples.size();
            samples.resize(at + static_cast<std::size_t>(chunk * channels));
            ma_uint64 read = 0;
            const ma_result result = ma_decoder_read_pcm_frames(&decoder, samples.data() + at, chunk, &read);
            samples.resize(at + static_cast<std::size_t>(read * channels));
            if (result != MA_SUCCESS || read < chunk) {
                break;
            }
        }
        ma_decoder_uninit(&decoder);

        if (samples.empty() || channels == 0 || rate == 0) {
            return std::unexpected{tgx::Error::decode};
        }
        return tgx::Sound::from_samples(samples, static_cast<int>(channels), static_cast<int>(rate));
    }

    // miniaudio takes paths as char, or on Windows as wchar_t, which is what
    // a path holds there (non-ASCII names included).
    [[nodiscard]] auto init_stream(ma_engine &engine, const std::filesystem::path &path, ma_sound &sound) -> ma_result {
        constexpr ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
#if defined(_WIN32)
        return ma_sound_init_from_file_w(&engine, path.c_str(), flags, nullptr, nullptr, &sound);
#else
        return ma_sound_init_from_file(&engine, path.c_str(), flags, nullptr, nullptr, &sound);
#endif
    }

    // Whether the file is there and opens for reading: what Error::io means,
    // told apart before miniaudio, which reports both alike, decodes it.
    [[nodiscard]] auto readable(const std::filesystem::path &path) -> bool {
        std::error_code err;
        return std::filesystem::is_regular_file(path, err) && std::ifstream{path, std::ios::binary}.is_open();
    }

    // The length in seconds, by a decoder reading the whole file from
    // memory: a stream reads Vorbis in a way that cannot tell it, and so
    // does a decoder opened on the path on Windows. 0 if it does not tell.
    [[nodiscard]] auto measure(const std::filesystem::path &path) -> tgx::Result<double> {
        return tgx::detail::read_file(path).and_then([](const std::vector<std::byte> &bytes) -> tgx::Result<double> {
            const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
            ma_decoder decoder;
            if (ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder) != MA_SUCCESS) {
                return std::unexpected{tgx::Error::decode};
            }
            ma_uint64 frames = 0;
            ma_uint32 rate = 0;
            double length = 0.0;
            ma_decoder_get_data_format(&decoder, nullptr, nullptr, &rate, nullptr, 0);
            if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) == MA_SUCCESS && rate != 0) {
                length = static_cast<double>(frames) / static_cast<double>(rate);
            }
            ma_decoder_uninit(&decoder);
            return length;
        });
    }
}

namespace tgx {
    // Sound

    auto Sound::load(const std::filesystem::path &path) -> Result<Sound> {
        return detail::read_file(path).and_then([](const std::vector<std::byte> &bytes) {
            return decode(bytes);
        });
    }

    auto Sound::decode(std::span<const std::byte> encoded) -> Result<Sound> {
        return decode_all(encoded);
    }

    auto Sound::from_samples(std::span<const float> samples, int channels, int sample_rate) -> Sound {
        // 254: miniaudio's MA_MAX_CHANNELS.
        TGX_ASSERT_MSG(
            channels > 0 && channels <= 254 && sample_rate > 0,
            "sound of {} channels at {} Hz",
            channels, sample_rate
        );
        TGX_ASSERT_MSG(
            samples.size() % static_cast<std::size_t>(channels) == 0,
            "{} samples do not split into {} channels",
            samples.size(), channels
        );

        return Sound{std::vector<float>(samples.begin(), samples.end()), channels, sample_rate};
    }

    Sound::Sound(std::vector<float> samples, int channels, int sample_rate) noexcept
        : m_samples{std::move(samples)},
          m_channels{channels},
          m_sample_rate{sample_rate},
          m_id{s_next_sound_id.fetch_add(1, std::memory_order_relaxed)} {
    }

    Sound::Sound(Sound &&other) noexcept
        : m_samples{std::move(other.m_samples)},
          m_channels{other.m_channels},
          m_sample_rate{other.m_sample_rate},
          m_id{std::exchange(other.m_id, 0)} {
    }

    auto Sound::operator=(Sound &&other) noexcept -> Sound & {
        if (this == &other) {
            return *this;
        }
        release();
        m_samples = std::move(other.m_samples);
        m_channels = other.m_channels;
        m_sample_rate = other.m_sample_rate;
        m_id = std::exchange(other.m_id, 0);
        return *this;
    }

    Sound::~Sound() {
        release();
    }

    auto Sound::release() noexcept -> void {
        // Its voices read its samples: they stop before the samples go.
        stop_voices(m_id);
        m_id = 0;
    }

    auto Sound::duration() const noexcept -> double {
        if (m_channels == 0 || m_sample_rate == 0) {
            return 0.0;
        }
        const auto frames = m_samples.size() / static_cast<std::size_t>(m_channels);
        return static_cast<double>(frames) / static_cast<double>(m_sample_rate);
    }

    // Music

    auto Music::load(const std::filesystem::path &path) -> Result<Music> {
        if (!readable(path)) {
            return std::unexpected{Error::io};
        }

        auto stream = std::make_unique<detail::MusicStream>();
        if (s_audio) {
            if (init_stream(s_audio->engine, path, stream->sound) != MA_SUCCESS) {
                return std::unexpected{Error::decode};
            }
            stream->live = true;
            float length = 0.f;
            if (ma_sound_get_length_in_seconds(&stream->sound, &length) == MA_SUCCESS && length > 0.f) {
                stream->length = length;
            } else {
                stream->length = measure(path).value_or(0.0);
            }
            return Music{std::move(stream)};
        }

        // Silent: the file is still checked and measured, so a game behaves
        // the same with and without sound.
        const Result<double> length = measure(path);
        if (!length) {
            return std::unexpected{length.error()};
        }
        stream->length = *length;
        return Music{std::move(stream)};
    }

    Music::Music(std::unique_ptr<detail::MusicStream> stream) noexcept : m_stream{std::move(stream)} {
    }

    Music::Music(Music &&) noexcept = default;

    auto Music::operator=(Music &&) noexcept -> Music & = default;

    Music::~Music() = default;

    auto Music::play() noexcept -> void {
        if (m_stream && m_stream->live) {
            ma_sound_start(&m_stream->sound);
        }
    }

    auto Music::pause() noexcept -> void {
        if (m_stream && m_stream->live) {
            ma_sound_stop(&m_stream->sound);
        }
    }

    auto Music::stop() noexcept -> void {
        if (m_stream && m_stream->live) {
            ma_sound_stop(&m_stream->sound);
            ma_sound_seek_to_pcm_frame(&m_stream->sound, 0);
        }
    }

    auto Music::playing() const noexcept -> bool {
        return m_stream && m_stream->live && ma_sound_is_playing(&m_stream->sound);
    }

    auto Music::set_looping(bool looping) noexcept -> void {
        if (m_stream && m_stream->live) {
            ma_sound_set_looping(&m_stream->sound, looping ? MA_TRUE : MA_FALSE);
        }
    }

    auto Music::set_volume(float volume) noexcept -> void {
        if (m_stream && m_stream->live) {
            ma_sound_set_volume(&m_stream->sound, std::max(volume, 0.f));
        }
    }

    auto Music::seek(double seconds) noexcept -> void {
        if (m_stream && m_stream->live) {
            ma_sound_seek_to_second(&m_stream->sound, static_cast<float>(std::max(seconds, 0.0)));
        }
    }

    auto Music::position() const noexcept -> double {
        float cursor = 0.f;
        if (m_stream && m_stream->live && ma_sound_get_cursor_in_seconds(&m_stream->sound, &cursor) == MA_SUCCESS) {
            return cursor;
        }
        return 0.0;
    }

    auto Music::length() const noexcept -> double {
        return m_stream ? m_stream->length : 0.0;
    }

    // Audio

    auto Audio::create() noexcept -> Audio {
        auto state = std::make_unique<AudioState>();
        const ma_engine_config config = ma_engine_config_init();
        if (ma_engine_init(&config, &state->engine) != MA_SUCCESS) {
            detail::log_warn("audio: no output device, sound stays silent");
            return Audio{};
        }

        ma_device *device = ma_engine_get_device(&state->engine);
        ma_device_info info{};
        ma_device_get_info(device, ma_device_type_playback, &info);
        detail::log_info(
            "audio: {} via {}, {} Hz, {} channels",
            info.name[0] != '\0' ? info.name : "default device",
            ma_get_backend_name(device->pContext->backend),
            ma_engine_get_sample_rate(&state->engine),
            ma_engine_get_channels(&state->engine)
        );

        ma_engine_set_volume(&state->engine, s_volume);
        s_audio = std::move(state);
        return Audio{};
    }

    auto Audio::operator=(Audio &&other) noexcept -> Audio & {
        if (this == &other) {
            return *this;
        }
        release();
        m_owned = std::exchange(other.m_owned, false);
        return *this;
    }

    Audio::~Audio() {
        release();
    }

    auto Audio::release() noexcept -> void {
        if (!m_owned) {
            return;
        }
        if (s_audio) {
            for (Voice &voice: s_audio->voices) {
                if (voice.owner != 0) {
                    free_voice(voice);
                }
            }
            ma_engine_uninit(&s_audio->engine);
            s_audio.reset();
        }
        m_owned = false;
    }

    auto Audio::active() const noexcept -> bool {
        return static_cast<bool>(s_audio);
    }

    auto Audio::play(const Sound &sound, const PlayParams &params) noexcept -> void {
        // miniaudio would keep the pitch it had without a word.
        TGX_ASSERT_MSG(params.pitch > 0.f, "pitch {}: playing at no speed or backwards", params.pitch);

        if (!s_audio || sound.m_id == 0 || sound.m_samples.empty()) {
            return;
        }

        Voice &voice = take_voice(*s_audio);
        const auto frames = sound.m_samples.size() / static_cast<std::size_t>(sound.m_channels);
        if (ma_audio_buffer_ref_init(
                ma_format_f32,
                static_cast<ma_uint32>(sound.m_channels),
                sound.m_samples.data(),
                frames,
                &voice.data
            ) != MA_SUCCESS) {
            return;
        }
        // The ref takes no rate; the engine resamples from this one.
        voice.data.sampleRate = static_cast<ma_uint32>(sound.m_sample_rate);

        if (ma_sound_init_from_data_source(
                &s_audio->engine,
                &voice.data,
                MA_SOUND_FLAG_NO_SPATIALIZATION,
                nullptr,
                &voice.sound
            ) != MA_SUCCESS) {
            ma_audio_buffer_ref_uninit(&voice.data);
            return;
        }

        ma_sound_set_volume(&voice.sound, std::max(params.volume, 0.f));
        ma_sound_set_pan(&voice.sound, std::clamp(params.pan, -1.f, 1.f));
        ma_sound_set_pitch(&voice.sound, params.pitch);
        voice.owner = sound.m_id;
        voice.started = ++s_audio->plays;
        ma_sound_start(&voice.sound);
    }

    auto Audio::stop(const Sound &sound) noexcept -> void {
        stop_voices(sound.m_id);
    }

    auto Audio::set_volume(float volume) noexcept -> void {
        s_volume = std::max(volume, 0.f);
        if (s_audio) {
            ma_engine_set_volume(&s_audio->engine, s_volume);
        }
    }

    auto Audio::volume() const noexcept -> float {
        return s_volume;
    }
}
