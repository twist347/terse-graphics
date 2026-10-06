# doctest v2.5.3, https://github.com/doctest/doctest
# For tgx's own tests only: nothing in the library links it, so its MIT notice
# never travels with a game.
add_library(tgx_doctest INTERFACE)
target_include_directories(tgx_doctest SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
