function(target_treat_all_warnings_as_errors Target)
	# Treat warnings as errors
	set_target_properties(${Target} PROPERTIES COMPILE_WARNING_AS_ERROR ON)

	# Turn all warnings on
	if (MSVC)
		target_compile_options(${Target} PRIVATE /W4)
	else()
		# stb_image_write.h does not work with -Wextra due to WD4700
		# target_compile_options(${Target} PRIVATE -Wall -Wextra -pedantic)
		target_compile_options(${Target} PRIVATE -Wall -pedantic)
	endif()
endfunction()