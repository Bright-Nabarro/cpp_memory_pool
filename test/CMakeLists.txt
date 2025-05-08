set(output_dir "${CMAKE_SOURCE_DIR}/test/bin")
function(set_test_output target)
	set_target_properties(${target}
		PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${output_dir}
	)
endfunction()
add_subdirectory(unit_test)
add_subdirectory(simple)
