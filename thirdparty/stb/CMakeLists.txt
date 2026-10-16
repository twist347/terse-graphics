# stb_image v2.30, https://github.com/nothings/stb
# Header only here: src/core/image.cpp compiles it with internal linkage.
# stb_vorbis v1.22 sits next to it, compiled by src/core/audio.cpp.
add_library(tgx_stb_image INTERFACE)
target_include_directories(tgx_stb_image SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Files are read by the caller, which handles paths stdio cannot (non-ASCII
# ones on Windows); only the formats 2D drawing needs are compiled in.
# Failure reasons are not read, so their strings are left out.
target_compile_definitions(tgx_stb_image INTERFACE
        STBI_NO_STDIO
        STBI_NO_FAILURE_STRINGS
        STBI_ONLY_PNG
        STBI_ONLY_JPEG
        STBI_ONLY_BMP
        STBI_ONLY_TGA
        STBI_ONLY_GIF
)

# stb_image_write v1.16, the copy GLFW ships in deps/: PNGs for Image::save.
# Written to memory, and to the file by the caller, as stb_image's reading.
add_library(tgx_stb_image_write INTERFACE)
target_include_directories(tgx_stb_image_write SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(tgx_stb_image_write INTERFACE STBI_WRITE_NO_STDIO)
