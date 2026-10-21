// A shared library with tgx inside: built only, never loaded. Uses parts that
// pull in the window, GL and audio code, so all of them must link into it.

#include "tgx/tgx.hpp"

#if defined(_WIN32)
#define TGX_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define TGX_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

TGX_PLUGIN_EXPORT auto tgx_plugin_frame() -> int {
    auto app = tgx::App::create();
    if (!app) {
        return 1;
    }
    app->canvas().rect({0, 0, 10, 10}, tgx::colors::red);
    app->swap_buffers();
    return 0;
}
