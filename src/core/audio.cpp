#include "tgx/core/audio.h"

#include "tgx/core/assert.h"

#include "core/file.h"
#include "core/log_internal.h"

#include <miniaudio/miniaudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace {
    // Plays of Sounds at once. Each holds a sound of miniaudio's, which may
    // not move while it plays, so they live in a fixed pool, with one voice
    // to spare: a new play starts in it before the oldest is cut off for it,
    // so a play that fails to start cuts off nothing.
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
        std::array<Voice, max_voices + 1> voices;
        std::uint64_t plays{0};
    };

    // Null when there is no Audio or it stays silent.
    std::unique_ptr<AudioState> s_audio;
    // As last set, kept while silent too, so volume() answers the same; back
    // to 1 when the Audio goes.
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

    // A free voice: there is always one, as at most max_voices play.
    [[nodiscard]] auto take_voice(AudioState &audio) noexcept -> Voice & {
        reclaim_voices(audio);
        return *std::ranges::find(audio.voices, 0u, &Voice::owner);
    }

    // Once a play has started in the spare voice: the one that has played
    // longest cut off if more than max_voices play now.
    auto keep_to_max(AudioState &audio) noexcept -> void {
        const auto playing = std::ranges::count_if(audio.voices, [](const Voice &voice) { return voice.owner != 0; });
        if (static_cast<std::size_t>(playing) <= max_voices) {
            return;
        }
        Voice &oldest = *std::ranges::min_element(audio.voices, {}, [](const Voice &voice) {
            return voice.owner != 0 ? voice.started : UINT64_MAX;
        });
        free_voice(oldest);
    }

    // A Sound with no id (moved from) touches nothing shared, so loading one
    // on another thread, where the temporary inside load goes, is safe.
    auto stop_voices(std::uint32_t owner) noexcept -> void {
        if (owner == 0 || !s_audio) {
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
        double duration{0.0};
        // As set, kept for a silent stream too.
        bool looping{false};
        float volume{1.f};

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
    // A decoder over the bytes of a sound file in memory, giving 32-bit
    // floats, and otherwise the sound as it was recorded: its own rate and
    // channels, which the engine converts as it plays. Let go however its
    // scope ends, an exception included.
    struct MemoryDecoder {
        ma_decoder decoder{};
        bool open{false};

        explicit MemoryDecoder(std::span<const std::byte> bytes) noexcept {
            const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
            open = ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder) == MA_SUCCESS;
        }

        MemoryDecoder(const MemoryDecoder &) = delete;
        auto operator=(const MemoryDecoder &) -> MemoryDecoder & = delete;

        ~MemoryDecoder() {
            if (open) {
                ma_decoder_uninit(&decoder);
            }
        }
    };

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

    // The duration in seconds, by a decoder reading the whole file from
    // memory: a stream reads Vorbis in a way that cannot tell it, and so
    // does a decoder opened on the path on Windows. 0 if it does not tell.
    [[nodiscard]] auto measure(const std::filesystem::path &path) -> tgx::Result<double> {
        return tgx::detail::read_file(path).and_then([](const std::vector<std::byte> &bytes) -> tgx::Result<double> {
            MemoryDecoder source{bytes};
            if (!source.open) {
                return std::unexpected{tgx::Error::decode};
            }
            ma_uint64 frames = 0;
            ma_uint32 rate = 0;
            ma_decoder_get_data_format(&source.decoder, nullptr, nullptr, &rate, nullptr, 0);
            if (ma_decoder_get_length_in_pcm_frames(&source.decoder, &frames) != MA_SUCCESS || rate == 0) {
                return 0.0;
            }
            return static_cast<double>(frames) / static_cast<double>(rate);
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

    // All of it, as 32-bit floats at the rate and channels it has. A file
    // with no samples in it is no sound either, and one broken partway is
    // not a shorter one.
    auto Sound::decode(std::span<const std::byte> encoded) -> Result<Sound> {
        MemoryDecoder source{encoded};
        if (!source.open) {
            return std::unexpected{Error::decode};
        }
        ma_decoder &decoder = source.decoder;

        ma_uint32 channels = 0;
        ma_uint32 rate = 0;
        ma_decoder_get_data_format(&decoder, nullptr, &channels, &rate, nullptr, 0);
        if (channels == 0 || rate == 0) {
            return std::unexpected{Error::decode};
        }

        // In chunks: some formats (Vorbis) cannot tell their length up front.
        // Those that can get room for all of it, and the last chunk, at once.
        std::vector<float> samples;
        constexpr ma_uint64 chunk = 4096;
        if (ma_uint64 frames = 0; ma_decoder_get_length_in_pcm_frames(&decoder, &frames) == MA_SUCCESS) {
            samples.reserve(static_cast<std::size_t>((frames + chunk) * channels));
        }
        for (;;) {
            const std::size_t at = samples.size();
            samples.resize(at + static_cast<std::size_t>(chunk * channels));
            ma_uint64 read = 0;
            const ma_result result = ma_decoder_read_pcm_frames(&decoder, samples.data() + at, chunk, &read);
            samples.resize(at + static_cast<std::size_t>(read * channels));
            if (result != MA_SUCCESS && result != MA_AT_END) {
                return std::unexpected{Error::decode};
            }
            if (result == MA_AT_END || read < chunk) {
                break;
            }
        }

        if (samples.empty()) {
            return std::unexpected{Error::decode};
        }
        // Moved in, not copied as from_samples does: one sound's worth of
        // memory at a time.
        return Sound{std::move(samples), static_cast<int>(channels), static_cast<int>(rate)};
    }

    auto Sound::from_samples(std::span<const float> samples, int channels, int sample_rate) -> Sound {
        TGX_ASSERT_MSG(
            channels > 0 && channels <= MA_MAX_CHANNELS && sample_rate > 0,
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

    auto Sound::duration() const noexcept -> double {
        if (m_channels == 0 || m_sample_rate == 0) {
            return 0.0;
        }
        const auto frames = m_samples.size() / static_cast<std::size_t>(m_channels);
        return static_cast<double>(frames) / static_cast<double>(m_sample_rate);
    }

    Sound::Sound(std::vector<float> samples, int channels, int sample_rate) noexcept
        : m_samples{std::move(samples)},
          m_channels{channels},
          m_sample_rate{sample_rate},
          m_id{s_next_sound_id.fetch_add(1, std::memory_order_relaxed)} {
    }

    auto Sound::release() noexcept -> void {
        // Its voices read its samples: they stop before the samples go.
        stop_voices(m_id);
        m_id = 0;
    }

    // Music

    auto Music::load(const std::filesystem::path &path) -> Result<Music> {
        // Told apart before miniaudio, which reports a missing file and one it
        // cannot decode alike.
        if (!detail::readable(path)) {
            return std::unexpected{Error::io};
        }

        auto stream = std::make_unique<detail::MusicStream>();
        if (s_audio) {
            if (init_stream(s_audio->engine, path, stream->sound) != MA_SUCCESS) {
                return std::unexpected{Error::decode};
            }
            stream->live = true;
            float seconds = 0.f;
            if (ma_sound_get_length_in_seconds(&stream->sound, &seconds) == MA_SUCCESS && seconds > 0.f) {
                stream->duration = seconds;
            } else {
                stream->duration = measure(path).value_or(0.0);
            }
            return Music{std::move(stream)};
        }

        // Silent: the file is still checked and measured, so a game behaves
        // the same with and without sound.
        const Result<double> seconds = measure(path);
        if (!seconds) {
            return std::unexpected{seconds.error()};
        }
        stream->duration = *seconds;
        return Music{std::move(stream)};
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
        if (!m_stream) {
            return;
        }
        m_stream->looping = looping;
        if (m_stream->live) {
            ma_sound_set_looping(&m_stream->sound, looping ? MA_TRUE : MA_FALSE);
        }
    }

    auto Music::looping() const noexcept -> bool {
        return m_stream && m_stream->looping;
    }

    auto Music::set_volume(float volume) noexcept -> void {
        // One NaN reaches the shared mix and silences everything.
        TGX_ASSERT_MSG(std::isfinite(volume) && volume >= 0.f, "volume {}: not 0 or more", volume);

        if (!m_stream) {
            return;
        }
        m_stream->volume = volume;
        if (m_stream->live) {
            ma_sound_set_volume(&m_stream->sound, m_stream->volume);
        }
    }

    auto Music::volume() const noexcept -> float {
        return m_stream ? m_stream->volume : 1.f;
    }

    auto Music::seek(double seconds) noexcept -> void {
        TGX_ASSERT_MSG(std::isfinite(seconds) && seconds >= 0.0, "seek({}): not a second of it", seconds);

        if (!m_stream || !m_stream->live) {
            return;
        }
        ma_sound &sound = m_stream->sound;
        // Frames at the engine's rate, as miniaudio counts a sound's.
        const ma_uint32 rate = ma_engine_get_sample_rate(ma_sound_get_engine(&sound));
        auto frame = static_cast<ma_uint64>(seconds * rate);

        // miniaudio takes a frame past the end around the length, and one two
        // lengths past leaves the sound at its end for good: the end stops a
        // piece played once, a looping one goes round. A stream that cannot
        // tell its length (Vorbis) has the duration measured at load.
        ma_uint64 length = 0;
        if (ma_sound_get_length_in_pcm_frames(&sound, &length) != MA_SUCCESS || length == 0) {
            length = static_cast<ma_uint64>(m_stream->duration * rate);
        }
        if (length > 0) {
            frame = m_stream->looping ? frame % length : std::min(frame, length);
        }
        ma_sound_seek_to_pcm_frame(&sound, frame);
    }

    auto Music::position() const noexcept -> double {
        float cursor = 0.f;
        if (m_stream && m_stream->live && ma_sound_get_cursor_in_seconds(&m_stream->sound, &cursor) == MA_SUCCESS) {
            return cursor;
        }
        return 0.0;
    }

    auto Music::duration() const noexcept -> double {
        return m_stream ? m_stream->duration : 0.0;
    }

    Music::Music(std::unique_ptr<detail::MusicStream> stream) noexcept : m_stream{std::move(stream)} {
    }

    // Audio

    auto Audio::create() noexcept -> Audio {
        // No memory for it is one more way to stay silent, not to fail.
        std::unique_ptr<AudioState> state{new (std::nothrow) AudioState{}};
        if (!state) {
            detail::log_warn("audio: out of memory, sound stays silent");
            return Audio{};
        }
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

    auto Audio::active() const noexcept -> bool {
        return static_cast<bool>(s_audio);
    }

    auto Audio::play(const Sound &sound, const PlayParams &params) noexcept -> void {
        // miniaudio would keep the pitch it had without a word.
        TGX_ASSERT_MSG(
            std::isfinite(params.pitch) && params.pitch > 0.f,
            "pitch {}: not a speed to play at",
            params.pitch
        );
        // One NaN reaches the shared mix and silences everything.
        TGX_ASSERT_MSG(std::isfinite(params.volume) && params.volume >= 0.f, "volume {}: not 0 or more", params.volume);
        TGX_ASSERT_MSG(params.pan >= -1.f && params.pan <= 1.f, "pan {}: not from -1 to 1", params.pan);

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

        ma_sound_set_volume(&voice.sound, params.volume);
        ma_sound_set_pan(&voice.sound, params.pan);
        ma_sound_set_pitch(&voice.sound, params.pitch);
        voice.owner = sound.m_id;
        voice.started = ++s_audio->plays;
        ma_sound_start(&voice.sound);
        keep_to_max(*s_audio);
    }

    auto Audio::stop(const Sound &sound) noexcept -> void {
        stop_voices(sound.m_id);
    }

    auto Audio::set_volume(float volume) noexcept -> void {
        // One NaN reaches the shared mix and silences everything.
        TGX_ASSERT_MSG(std::isfinite(volume) && volume >= 0.f, "volume {}: not 0 or more", volume);

        s_volume = volume;
        if (s_audio) {
            ma_engine_set_volume(&s_audio->engine, s_volume);
        }
    }

    auto Audio::volume() const noexcept -> float {
        return s_volume;
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
        s_volume = 1.f;
        m_owned = false;
    }
}
