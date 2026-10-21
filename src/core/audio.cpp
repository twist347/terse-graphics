#include "tgx/core/audio.hpp"

#include "tgx/core/assert.hpp"

#include "core/file.hpp"
#include "core/log_internal.hpp"
#include "core/tone.hpp"

// miniaudio and stb_vorbis are compiled here, private to tgx, so an app may
// link its own copies. MA_API makes miniaudio's functions static. stb_vorbis
// has no such switch, so its functions, and miniaudio's one global, get a
// tgx_ prefix. miniaudio includes <windows.h>, whose min and max macros would
// break std::min.
#include "core/vendor_begin.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define MA_API static
#define ma_atomic_global_lock tgx_ma_atomic_global_lock
#define stb_vorbis_close tgx_stb_vorbis_close
#define stb_vorbis_decode_filename tgx_stb_vorbis_decode_filename
#define stb_vorbis_decode_frame_pushdata tgx_stb_vorbis_decode_frame_pushdata
#define stb_vorbis_decode_memory tgx_stb_vorbis_decode_memory
#define stb_vorbis_flush_pushdata tgx_stb_vorbis_flush_pushdata
#define stb_vorbis_get_comment tgx_stb_vorbis_get_comment
#define stb_vorbis_get_error tgx_stb_vorbis_get_error
#define stb_vorbis_get_file_offset tgx_stb_vorbis_get_file_offset
#define stb_vorbis_get_frame_float tgx_stb_vorbis_get_frame_float
#define stb_vorbis_get_frame_short tgx_stb_vorbis_get_frame_short
#define stb_vorbis_get_frame_short_interleaved tgx_stb_vorbis_get_frame_short_interleaved
#define stb_vorbis_get_info tgx_stb_vorbis_get_info
#define stb_vorbis_get_sample_offset tgx_stb_vorbis_get_sample_offset
#define stb_vorbis_get_samples_float tgx_stb_vorbis_get_samples_float
#define stb_vorbis_get_samples_float_interleaved tgx_stb_vorbis_get_samples_float_interleaved
#define stb_vorbis_get_samples_short tgx_stb_vorbis_get_samples_short
#define stb_vorbis_get_samples_short_interleaved tgx_stb_vorbis_get_samples_short_interleaved
#define stb_vorbis_open_file tgx_stb_vorbis_open_file
#define stb_vorbis_open_file_section tgx_stb_vorbis_open_file_section
#define stb_vorbis_open_filename tgx_stb_vorbis_open_filename
#define stb_vorbis_open_memory tgx_stb_vorbis_open_memory
#define stb_vorbis_open_pushdata tgx_stb_vorbis_open_pushdata
#define stb_vorbis_seek tgx_stb_vorbis_seek
#define stb_vorbis_seek_frame tgx_stb_vorbis_seek_frame
#define stb_vorbis_seek_start tgx_stb_vorbis_seek_start
#define stb_vorbis_stream_length_in_samples tgx_stb_vorbis_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds tgx_stb_vorbis_stream_length_in_seconds
// Its declarations go before miniaudio's implementation, its own after.
#define STB_VORBIS_HEADER_ONLY
#include <stb/stb_vorbis.c>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>
#undef STB_VORBIS_HEADER_ONLY
#include <stb/stb_vorbis.c>
#include "core/vendor_end.hpp"

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
    // A miniaudio sound must not move while it plays, so voices live in a
    // fixed pool. One spare voice: a new play starts there before the oldest
    // is cut off, so a play that fails to start cuts off nothing.
    constexpr std::size_t max_voices = 64;

    // One play of a Sound. data reads its samples in place.
    struct Voice {
        ma_audio_buffer_ref data{};
        ma_sound sound{};
        // Sound id; 0 while free.
        std::uint32_t owner{0};
        // Play counter at start. The oldest is cut off first.
        std::uint64_t started{0};
    };

    // On the heap: neither the engine nor the voices may move.
    struct AudioState {
        ma_engine engine{};
        std::array<Voice, max_voices + 1> voices;
    };

    // Null with no Audio or a silent one.
    std::unique_ptr<AudioState> s_audio{};
    // Kept while silent too. Reset to 1 when the Audio is destroyed.
    float s_volume{1.f};
    // Counts plays over every Audio, so a Playback never matches a later
    // Audio's play.
    std::uint64_t s_plays{0};
    // Atomic: Sounds may be loaded on other threads.
    std::atomic<std::uint32_t> s_next_sound_id{1};

    auto free_voice(Voice &voice) noexcept -> void {
        ma_sound_uninit(&voice.sound);
        ma_audio_buffer_ref_uninit(&voice.data);
        voice.owner = 0;
    }

    // Frees voices that have played to their end.
    auto reclaim_voices(AudioState &audio) noexcept -> void {
        for (Voice &voice: audio.voices) {
            if (voice.owner != 0 && ma_sound_at_end(&voice.sound)) {
                free_voice(voice);
            }
        }
    }

    // There is always one: at most max_voices play.
    [[nodiscard]] auto take_voice(AudioState &audio) noexcept -> Voice & {
        reclaim_voices(audio);
        return *std::ranges::find(audio.voices, 0u, &Voice::owner);
    }

    // Called after a play started in the spare voice. With no voice free,
    // more than max_voices play: the oldest is cut off.
    auto keep_to_max(AudioState &audio) noexcept -> void {
        if (std::ranges::contains(audio.voices, 0u, &Voice::owner)) {
            return;
        }
        free_voice(*std::ranges::min_element(audio.voices, {}, &Voice::started));
    }

    // The voice still holding this play, or null. A play at its end is freed
    // here: miniaudio would start it over from the beginning.
    auto voice_of(std::uint32_t index, std::uint64_t play) noexcept -> Voice * {
        if (play == 0 || !s_audio) {
            return nullptr;
        }
        Voice &voice = s_audio->voices[index];
        if (voice.owner == 0 || voice.started != play) {
            return nullptr;
        }
        if (ma_sound_at_end(&voice.sound)) {
            free_voice(voice);
            return nullptr;
        }
        return &voice;
    }

    // Touches no shared state for id 0 (moved from). This keeps loading on
    // another thread safe: the temporary inside load is destroyed there.
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
    // live is false while the Audio is silent: then sound is not set up.
    struct MusicStream {
        ma_sound sound{};
        bool live{false};
        double duration{0.0};
        // Last set. Kept for a silent stream too.
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
    // One NaN would reach the shared mix and silence everything. Only used in
    // asserts.
    [[maybe_unused, nodiscard]] auto valid_volume(float volume) noexcept -> bool {
        return std::isfinite(volume) && volume >= 0.f;
    }

    // nullptr when the Music was moved from or the Audio is silent.
    [[nodiscard]] auto live_sound(const std::unique_ptr<tgx::detail::MusicStream> &stream) noexcept -> ma_sound * {
        return stream && stream->live ? &stream->sound : nullptr;
    }

    // 32-bit floats at the file's own rate and channels; the engine converts
    // while playing. Closed however the scope ends, exceptions included.
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

    // wchar_t on Windows, so non-ASCII names work.
    [[nodiscard]] auto init_stream(ma_engine &engine, const std::filesystem::path &path, ma_sound &sound) -> ma_result {
        constexpr ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
#if defined(_WIN32)
        return ma_sound_init_from_file_w(&engine, path.c_str(), flags, nullptr, nullptr, &sound);
#else
        return ma_sound_init_from_file(&engine, path.c_str(), flags, nullptr, nullptr, &sound);
#endif
    }

    // Seconds, by decoding the whole file from memory. A stream cannot report
    // it for Vorbis, nor can a decoder opened on the path on Windows. 0 if the
    // file does not report it.
    [[nodiscard]] auto duration_of(std::span<const std::byte> encoded) -> tgx::Result<double> {
        MemoryDecoder source{encoded};
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
    }

    [[nodiscard]] auto measure(const std::filesystem::path &path) -> tgx::Result<double> {
        return tgx::detail::read_file(path).and_then(duration_of);
    }
}

namespace tgx {
    // Sound

    auto Sound::load(const std::filesystem::path &path) -> Result<Sound> {
        return detail::read_file(path).and_then(decode);
    }

    // A file with no samples fails, and so does one broken partway; it does
    // not become a shorter sound.
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

        // In chunks: Vorbis cannot report its length up front. When the
        // length is known, all of it plus one chunk is reserved, unless it is
        // more than the bytes could hold: a broken file's header may claim
        // terabytes.
        std::vector<float> samples{};
        constexpr ma_uint64 chunk = 4096;
        constexpr ma_uint64 samples_per_byte = 1024;
        const ma_uint64 plausible = encoded.size() * samples_per_byte / channels;
        if (ma_uint64 frames = 0;
            ma_decoder_get_length_in_pcm_frames(&decoder, &frames) == MA_SUCCESS && frames <= plausible) {
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
        // Moved, not copied, so only one copy is in memory.
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

    auto Sound::tone(const ToneParams &params) -> Sound {
        return Sound{detail::tone_samples(params), 1, detail::tone_rate};
    }

    Sound::Sound(Sound &&other) noexcept
        : samples_{std::move(other.samples_)},
          channels_{other.channels_},
          sample_rate_{other.sample_rate_},
          id_{std::exchange(other.id_, 0)} {
    }

    auto Sound::operator=(Sound &&other) noexcept -> Sound & {
        if (this == &other) {
            return *this;
        }
        release();
        samples_ = std::move(other.samples_);
        channels_ = other.channels_;
        sample_rate_ = other.sample_rate_;
        id_ = std::exchange(other.id_, 0);
        return *this;
    }

    Sound::~Sound() {
        release();
    }

    auto Sound::duration() const noexcept -> double {
        if (channels_ == 0 || sample_rate_ == 0) {
            return 0.0;
        }
        const auto frames = samples_.size() / static_cast<std::size_t>(channels_);
        return static_cast<double>(frames) / static_cast<double>(sample_rate_);
    }

    Sound::Sound(std::vector<float> samples, int channels, int sample_rate) noexcept
        : samples_{std::move(samples)},
          channels_{channels},
          sample_rate_{sample_rate},
          id_{s_next_sound_id.fetch_add(1, std::memory_order_relaxed)} {
    }

    auto Sound::release() noexcept -> void {
        // Voices read the samples, so they stop before the samples are freed.
        stop_voices(id_);
        id_ = 0;
    }

    // Music

    auto Music::load(const std::filesystem::path &path) -> Result<Music> {
        // miniaudio reports a missing file and an undecodable one the same.
        if (!detail::readable(path)) {
            return std::unexpected{Error::io};
        }

        auto stream = std::make_unique<detail::MusicStream>();
        // Silent Audio: still check and measure, so a game behaves the same.
        if (!s_audio) {
            const Result<double> seconds = measure(path);
            if (!seconds) {
                return std::unexpected{seconds.error()};
            }
            stream->duration = *seconds;
            return Music{std::move(stream)};
        }

        if (init_stream(s_audio->engine, path, stream->sound) != MA_SUCCESS) {
            return std::unexpected{Error::decode};
        }
        stream->live = true;
        if (float seconds = 0.f;
            ma_sound_get_length_in_seconds(&stream->sound, &seconds) == MA_SUCCESS && seconds > 0.f) {
            stream->duration = seconds;
        } else {
            stream->duration = measure(path).value_or(0.0);
        }
        return Music{std::move(stream)};
    }

    Music::Music(Music &&) noexcept = default;

    auto Music::operator=(Music &&) noexcept -> Music & = default;

    Music::~Music() = default;

    auto Music::play() noexcept -> void {
        if (ma_sound *sound = live_sound(stream_)) {
            ma_sound_start(sound);
        }
    }

    auto Music::pause() noexcept -> void {
        if (ma_sound *sound = live_sound(stream_)) {
            ma_sound_stop(sound);
        }
    }

    auto Music::stop() noexcept -> void {
        if (ma_sound *sound = live_sound(stream_)) {
            ma_sound_stop(sound);
            ma_sound_seek_to_pcm_frame(sound, 0);
        }
    }

    auto Music::playing() const noexcept -> bool {
        const ma_sound *sound = live_sound(stream_);
        return sound && ma_sound_is_playing(sound);
    }

    auto Music::set_looping(bool looping) noexcept -> void {
        if (!stream_) {
            return;
        }
        stream_->looping = looping;
        if (ma_sound *sound = live_sound(stream_)) {
            ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);
        }
    }

    auto Music::looping() const noexcept -> bool {
        return stream_ && stream_->looping;
    }

    auto Music::set_volume(float volume) noexcept -> void {
        TGX_ASSERT_MSG(valid_volume(volume), "volume {}: not 0 or more", volume);

        if (!stream_) {
            return;
        }
        stream_->volume = volume;
        if (ma_sound *sound = live_sound(stream_)) {
            ma_sound_set_volume(sound, volume);
        }
    }

    auto Music::volume() const noexcept -> float {
        return stream_ ? stream_->volume : 1.f;
    }

    auto Music::seek(double seconds) noexcept -> void {
        TGX_ASSERT_MSG(std::isfinite(seconds) && seconds >= 0.0, "seek({}): not a second of it", seconds);

        ma_sound *sound = live_sound(stream_);
        if (!sound) {
            return;
        }
        // miniaudio counts a sound's frames at the engine's rate.
        const ma_uint32 rate = ma_engine_get_sample_rate(ma_sound_get_engine(sound));

        // miniaudio wraps a frame past the end around the length, and one two
        // lengths past leaves the sound stuck at its end, so clamp or wrap
        // here. Vorbis reports no length: use the duration measured at load.
        // In double: a huge time would overflow the frame count.
        ma_uint64 length = 0;
        if (ma_sound_get_length_in_pcm_frames(sound, &length) != MA_SUCCESS || length == 0) {
            length = static_cast<ma_uint64>(stream_->duration * rate);
        }
        double frame = seconds * rate;
        if (length > 0) {
            const auto end = static_cast<double>(length);
            frame = stream_->looping ? std::fmod(frame, end) : std::min(frame, end);
        } else {
            // Past the end of anything real, and still a whole ma_uint64.
            constexpr double far_frame = 0x1p62;
            frame = std::min(frame, far_frame);
        }
        ma_sound_seek_to_pcm_frame(sound, static_cast<ma_uint64>(frame));
    }

    auto Music::position() const noexcept -> double {
        const ma_sound *sound = live_sound(stream_);
        if (float cursor = 0.f; sound && ma_sound_get_cursor_in_seconds(sound, &cursor) == MA_SUCCESS) {
            return cursor;
        }
        return 0.0;
    }

    auto Music::duration() const noexcept -> double {
        return stream_ ? stream_->duration : 0.0;
    }

    Music::Music(std::unique_ptr<detail::MusicStream> stream) noexcept : stream_{std::move(stream)} {
    }

    // Playback

    auto Playback::stop() noexcept -> void {
        if (Voice *voice = voice_of(voice_, play_)) {
            free_voice(*voice);
        }
    }

    auto Playback::pause() noexcept -> void {
        if (Voice *voice = voice_of(voice_, play_)) {
            ma_sound_stop(&voice->sound);
        }
    }

    auto Playback::resume() noexcept -> void {
        if (Voice *voice = voice_of(voice_, play_)) {
            ma_sound_start(&voice->sound);
        }
    }

    auto Playback::playing() const noexcept -> bool {
        const Voice *voice = voice_of(voice_, play_);
        return voice && ma_sound_is_playing(&voice->sound);
    }

    // Audio

    auto Audio::create() noexcept -> Audio {
        // Out of memory: silent, not a failure.
        std::unique_ptr<AudioState> state{new (std::nothrow) AudioState{}};
        if (!state) {
            detail::log_warn("audio: out of memory, sound stays silent");
            return Audio{};
        }
        if (const ma_engine_config config = ma_engine_config_init();
            ma_engine_init(&config, &state->engine) != MA_SUCCESS) {
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

    auto Audio::active() const noexcept -> bool {
        return static_cast<bool>(s_audio);
    }

    auto Audio::play(const Sound &sound, const PlayParams &params) noexcept -> Playback {
        // miniaudio would silently keep the old pitch.
        TGX_ASSERT_MSG(
            std::isfinite(params.pitch) && params.pitch > 0.f,
            "pitch {}: not a speed to play at",
            params.pitch
        );
        TGX_ASSERT_MSG(valid_volume(params.volume), "volume {}: not 0 or more", params.volume);
        TGX_ASSERT_MSG(!std::isnan(params.pan), "pan {}: not a number", params.pan);

        if (!s_audio || sound.id_ == 0 || sound.samples_.empty()) {
            return {};
        }

        Voice &voice = take_voice(*s_audio);
        const auto frames = sound.samples_.size() / static_cast<std::size_t>(sound.channels_);
        if (ma_audio_buffer_ref_init(
                ma_format_f32,
                static_cast<ma_uint32>(sound.channels_),
                sound.samples_.data(),
                frames,
                &voice.data
            ) != MA_SUCCESS) {
            return {};
        }
        // The ref init takes no rate. The engine resamples from it.
        voice.data.sampleRate = static_cast<ma_uint32>(sound.sample_rate_);

        if (ma_sound_init_from_data_source(
                &s_audio->engine,
                &voice.data,
                MA_SOUND_FLAG_NO_SPATIALIZATION,
                nullptr,
                &voice.sound
            ) != MA_SUCCESS) {
            ma_audio_buffer_ref_uninit(&voice.data);
            return {};
        }

        ma_sound_set_volume(&voice.sound, params.volume);
        ma_sound_set_pan(&voice.sound, std::clamp(params.pan, -1.f, 1.f));
        ma_sound_set_pitch(&voice.sound, params.pitch);
        voice.owner = sound.id_;
        voice.started = ++s_plays;
        ma_sound_start(&voice.sound);
        keep_to_max(*s_audio);
        return {static_cast<std::uint32_t>(&voice - s_audio->voices.data()), voice.started};
    }

    auto Audio::stop(const Sound &sound) noexcept -> void {
        stop_voices(sound.id_);
    }

    auto Audio::set_volume(float volume) noexcept -> void {
        TGX_ASSERT_MSG(valid_volume(volume), "volume {}: not 0 or more", volume);

        s_volume = volume;
        if (s_audio) {
            ma_engine_set_volume(&s_audio->engine, s_volume);
        }
    }

    auto Audio::volume() const noexcept -> float {
        return s_volume;
    }

    auto detail::release_audio() noexcept -> void {
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
    }
}
