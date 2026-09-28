function(tgx_add_example name)
    add_executable(example_${name} ${name}.cpp)
    target_link_libraries(example_${name} PRIVATE tgx::tgx)
    target_compile_options(example_${name} PRIVATE ${TGX_WARNINGS})
endfunction()

tgx_add_example(01_window)
tgx_add_example(02_triangle)
