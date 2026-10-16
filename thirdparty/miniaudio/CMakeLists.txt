# miniaudio v0.11.25, https://github.com/mackron/miniaudio
# Public domain or MIT-0: no notice travels with a game. Header only here:
# src/core/audio.cpp compiles it with internal linkage, with OGG Vorbis
# decoding through stb_vorbis v1.22 (thirdparty/stb). Backends are loaded at
# run time on every platform, so no system audio library is linked.
add_library(tgx_miniaudio INTERFACE)
target_include_directories(tgx_miniaudio SYSTEM INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../stb/include
)
# Only playing back, nothing written or generated. No null backend either:
# without a real device the engine fails to start, and tgx says so instead of
# mixing into nothing.
target_compile_definitions(tgx_miniaudio INTERFACE MA_NO_ENCODING MA_NO_GENERATION MA_NO_NULL)
# Its system libraries (threads, dl, m) are linked by tgx, which compiles it.
