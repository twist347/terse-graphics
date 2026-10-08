// Sound: a note made in code, played at five pitches with keys 1 to 5,
// panned to where the mouse is. Given a music file (OGG, MP3, FLAC or WAV) on
// the command line, M plays and pauses it while it streams from the disk:
//
//     tgx_13_audio theme.ogg

#include "tgx/tgx.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <format>
#include <numbers>
#include <optional>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace {
    // A plucked note: a sine dying away, with a few milliseconds of fade in
    // so it starts without a click.
    [[nodiscard]] auto make_note() -> tgx::Sound {
        constexpr int rate = 44100;
        constexpr float frequency = 440.f;
        std::vector<float> samples(rate / 2);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const float t = static_cast<float>(i) / rate;
            const float fade_in = std::min(t / 0.005f, 1.f);
            const float decay = std::exp(-6.f * t);
            samples[i] = 0.5f * fade_in * decay * std::sin(2.f * std::numbers::pi_v<float> * frequency * t);
        }
        return tgx::Sound::from_samples(samples, 1, rate);
    }

    // A pentatonic scale: each pitch a ratio to the note as made.
    constexpr std::array keys{
        tgx::Key::digit1, tgx::Key::digit2, tgx::Key::digit3, tgx::Key::digit4, tgx::Key::digit5,
    };
    constexpr std::array pitches{1.f, 9.f / 8.f, 5.f / 4.f, 3.f / 2.f, 5.f / 3.f};
}

int main(int argc, char **argv) {
    // Info also prints what it runs on: tgx and the window, the GL context,
    // the audio output.
    tgx::set_log_level(tgx::LogLevel::info);

    auto app = tgx::App::create({.title = "tgx - 13 audio"});
    if (!app) {
        std::println(stderr, "app: {}", app.error());
        return 1;
    }

    auto &audio = app->audio();
    const tgx::Sound note = make_note();

    std::optional<tgx::Music> music;
    if (argc > 1) {
        auto loaded = tgx::Music::load(argv[1]);
        if (!loaded) {
            std::println(stderr, "music: {}", loaded.error());
            return 1;
        }
        music = std::move(*loaded);
        music->set_looping(true);
    }

    auto &canvas = app->canvas();
    const auto &input = app->input();
    std::array<float, 5> lit{};

    while (!app->should_close()) {
        app->poll_events();

        // -1 at the left edge of the window, 1 at the right.
        const float width = static_cast<float>(canvas.size().width);
        const float pan = std::clamp(input.mouse().x / width * 2.f - 1.f, -1.f, 1.f);

        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (input.pressed(keys[i])) {
                audio.play(note, {.volume = 0.8f, .pan = pan, .pitch = pitches[i]});
                lit[i] = 1.f;
            }
            lit[i] = std::max(lit[i] - 3.f * app->clock().delta(), 0.f);
        }
        if (music && input.pressed(tgx::Key::m)) {
            if (music->playing()) {
                music->pause();
            } else {
                music->play();
            }
        }

        canvas.clear(tgx::colors::dark_gray);

        canvas.text({40, 40}, audio.active() ? "audio: on" : "audio: off (no output device)", tgx::colors::white);
        canvas.text({40, 70}, "1-5: notes, panned to the mouse", tgx::colors::light_gray);

        for (std::size_t i = 0; i < keys.size(); ++i) {
            const tgx::Rect key{40.f + 90.f * static_cast<float>(i), 110, 80, 80};
            canvas.rect(key, tgx::lerp(tgx::colors::gray, tgx::colors::yellow, lit[i]));
            canvas.text({key.x + 34, key.y + 32}, std::format("{}", i + 1), tgx::colors::black);
        }

        // Where the notes will sound, from left to right.
        canvas.rect({40, 220, 440, 4}, tgx::colors::gray);
        canvas.circle({40 + (pan + 1.f) / 2.f * 440, 222}, 8, tgx::colors::yellow);

        if (music) {
            canvas.text({40, 260}, "M: play / pause the music", tgx::colors::light_gray);
            const char *state = music->playing() ? "playing" : "paused";
            const std::string where = std::format("{} {:.1f} / {:.1f} s", state, music->position(), music->duration());
            canvas.text({40, 290}, where, tgx::colors::white);
        } else {
            canvas.text({40, 260}, "no music file given", tgx::colors::light_gray);
        }

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
