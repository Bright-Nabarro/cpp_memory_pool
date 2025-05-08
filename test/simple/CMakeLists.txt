add_executable(simple_test "main.cpp")
target_link_libraries(simple_test memory_pool) 
set_test_output(simple_test)
add_test(simple_test "${output_dir}/simple_test")
