// Built against an installed tgx: the headers are found, the library and what
// it needs from the system link, and the parts that need no display run.
// App::create is only linked, not called, so the window, GL and audio code
// must link too.

#include "tgx/tgx.hpp"

#include <cstdio>
#include <print>

static_assert(TGX_ENABLE_ASSERTS == 0 || TGX_ENABLE_ASSERTS == 1);
#if !TGX_VERSION_AT_LEAST(0, 1, 0)
#error "the installed tgx is older than this check"
#endif

int main(int argc, char **) {
    if (argc > 1) {
        auto app = tgx::App::create();
        return app ? 0 : 1;
    }

    tgx::Random random{42};
    const tgx::Image image = tgx::Image::create({4, 4}, tgx::Color::hsv(random.next_float(0.f, 360.f), 1.f, 1.f));
    const bool inside = tgx::contains(tgx::Rect{0, 0, 4, 4}, {1, 1});
    if (image.size() != tgx::Size{4, 4} || !inside) {
        std::println(stderr, "tgx install check: wrong results");
        return 1;
    }
    std::println("tgx install check: ok (tgx {}, asserts {})", TGX_VERSION_STRING, TGX_ENABLE_ASSERTS != 0 ? "on" : "off");
    return 0;
}
