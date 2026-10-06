# Targets of our own copies are prefixed tgx_, so a project that adds tgx
# with add_subdirectory and has its own glad or stb keeps its names.
add_subdirectory(glad)
add_subdirectory(miniaudio)
add_subdirectory(stb)
if (TGX_BUILD_TESTS)
    add_subdirectory(doctest)
endif ()

# A project with a glfw target of its own (3.4 or newer: tgx asks which
# platform it runs on) keeps it, and tgx links that one. Otherwise ours, with
# its extras off: plain variables, which GLFW's options take over the cache
# without writing it, so nothing leaks into the project's settings.
if (NOT TARGET glfw)
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_BUILD_TESTS OFF)
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_INSTALL OFF)
    add_subdirectory(glfw-3.5.1 EXCLUDE_FROM_ALL)
endif ()
