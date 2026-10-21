// Sound: a note made in code, played at five pitches with keys 1 to 5,
// panned to where the mouse is. Space plays a long note that P pauses and
// resumes and S stops, leaving the others alone. Given a music file (OGG, MP3, FLAC or WAV) on
// the command line, M plays and pauses it while it streams from the disk:
//
//     tgx_13_audio theme.ogg

#include "tgx/tgx.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <utility>

namespace {
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
    // A plucked note: a sine dying away.
    const auto note = tgx::Sound::tone({.duration = 0.5f, .decay = 6});
    // A low hum that takes its time to fade.
    const auto drone = tgx::Sound::tone({
        .waveform = tgx::Waveform::triangle,
        .frequency = 110,
        .duration = 4,
        .volume = 0.4f,
        .decay = 0.5f,
    });
    tgx::Playback hum{};

    std::optional<tgx::Music> music{};
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
        const float pan = std::clamp(tgx::remap(input.mouse().x, 0, tgx::to_vec2(canvas.size()).x, -1, 1), -1.f, 1.f);

        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (input.pressed(keys[i])) {
                audio.play(note, {.volume = 0.8f, .pan = pan, .pitch = pitches[i]});
                lit[i] = 1.f;
            }
            lit[i] = std::max(lit[i] - 3.f * app->clock().delta(), 0.f);
        }
        if (input.pressed(tgx::Key::space)) {
            hum = audio.play(drone);
        }
        if (input.pressed(tgx::Key::p)) {
            if (hum.playing()) {
                hum.pause();
            } else {
                hum.resume();
            }
        }
        if (input.pressed(tgx::Key::s)) {
            hum.stop();
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
            canvas.text(key.center(), std::format("{}", i + 1), tgx::colors::black, tgx::Canvas::default_text_size,
                {0.5f, 0.5f});
        }

        // Where the notes will sound, from left to right.
        canvas.rect({40, 220, 440, 4}, tgx::colors::gray);
        canvas.circle({40 + (pan + 1.f) / 2.f * 440, 222}, 8, tgx::colors::yellow);

        canvas.text({40, 260}, "Space: a long note, P: pause / resume it, S: stop it", tgx::colors::light_gray);
        canvas.text({40, 290}, hum.playing() ? "the long note sounds" : "the long note is silent", tgx::colors::white);

        if (music) {
            canvas.text({40, 340}, "M: play / pause the music", tgx::colors::light_gray);
            const char *state = music->playing() ? "playing" : "paused";
            const std::string where = std::format("{} {:.1f} / {:.1f} s", state, music->position(), music->duration());
            canvas.text({40, 370}, where, tgx::colors::white);
        } else {
            canvas.text({40, 340}, "no music file given", tgx::colors::light_gray);
        }

        canvas.fps({10, 10});

        app->swap_buffers();
    }

    return 0;
}
