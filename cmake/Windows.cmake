function(enable_edit_and_continue target_name)
	# https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170#edit-and-continue-for-cmake-projects
	if(MSVC)
		target_compile_options(${target_name} PUBLIC "/ZI")
		target_link_options(${target_name} PUBLIC "/INCREMENTAL")
	endif()
endfunction()