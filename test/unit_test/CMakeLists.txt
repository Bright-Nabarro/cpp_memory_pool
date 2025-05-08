set(targets
	block_storage_test
	memory_pool_test
)

foreach(target ${targets})
	add_executable(${target} "${target}.cpp")
	target_link_libraries(${target} memory_pool)
	set_test_output(${target})
	add_test(${target} "${output_dir}/${target}")
endforeach()
