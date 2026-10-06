# Targets of our own copies are prefixed tgx_, so a project that adds tgx
# with add_subdirectory and has its own glad or stb keeps its names. They are
# object libraries, built into libtgx itself as raylib does: one library to
# link and to install.
add_subdirectory(glad)
add_subdirectory(miniaudio)
add_subdirectory(stb)
if (TGX_BUILD_TESTS)
    add_subdirectory(doctest)
endif ()

# A project with a glfw target of its own (3.4 or newer: tgx asks which
# platform it runs on) keeps it, and tgx links that one. Otherwise ours, as an
# object library too, with its extras off: plain variables, which GLFW's
# options take over the cache without writing it, so nothing leaks into the
# project's settings. GLFW_LIBRARY_TYPE alone is a cache string, which GLFW
# writes there as OBJECT; the variable decides either way.
if (TARGET glfw)
    set(TGX_OWN_GLFW OFF PARENT_SCOPE)
else ()
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_BUILD_TESTS OFF)
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_INSTALL OFF)
    set(GLFW_LIBRARY_TYPE OBJECT)
    add_subdirectory(glfw-3.5.1 EXCLUDE_FROM_ALL)
    set(TGX_OWN_GLFW ON PARENT_SCOPE)
endif ()
