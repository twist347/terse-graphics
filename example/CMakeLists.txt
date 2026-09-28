add_executable(example_01_window main.cpp)
target_link_libraries(example_01_window PRIVATE tgx::tgx)
target_compile_options(example_01_window PRIVATE ${TGX_WARNINGS})
