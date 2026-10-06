add_library(tgx_glad OBJECT src/gl.c)
target_include_directories(tgx_glad SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
