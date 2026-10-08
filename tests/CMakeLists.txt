# One doctest executable of tgx's tests, registered with CTest under its name
# and LABEL. Every test sees support.hpp, shared by all of them; INTERNAL also
# lets it include tgx's internal headers from src/ (only those that need
# nothing but public headers).
function(tgx_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "INTERNAL" "LABEL" "SOURCES")
    add_executable(${name} ${arg_SOURCES})
    target_link_libraries(${name} PRIVATE tgx::tgx tgx_doctest)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    if (arg_INTERNAL)
        target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    endif ()
    target_compile_options(${name} PRIVATE ${TGX_WARNINGS})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS ${arg_LABEL})
endfunction()

add_subdirectory(cpu)
add_subdirectory(gpu)
