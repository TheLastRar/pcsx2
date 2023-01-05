add_pcsx2_test(core_test
	StubHost.cpp
)

set(multi_isa_sources
	GS/swizzle_test_main.cpp
)

target_link_libraries(core_test PUBLIC
	PCSX2_FLAGS
	PCSX2
	common
)

if(DISABLE_ADVANCE_SIMD)
	if(WIN32)
		set(compile_options_avx2 /arch:AVX2)
		set(compile_options_avx  /arch:AVX)
	elseif(USE_GCC)
		# GCC can't inline into multi-isa functions if we use march and mtune, but can if we use feature flags
		set(compile_options_avx2 -msse4.1 -mavx -mavx2 -mbmi -mbmi2 -mfma)
		set(compile_options_avx  -msse4.1 -mavx)
		set(compile_options_sse4 -msse4.1)
	else()
		set(compile_options_avx2 -march=haswell -mtune=haswell)
		set(compile_options_avx  -march=sandybridge -mtune=sandybridge)
		set(compile_options_sse4 -msse4.1 -mtune=nehalem)
	endif()
	# ODR violation time!
	# Everything would be fine if we only defined things in cpp files, but C++ tends to like inline functions (STL anyone?)
	# Each ISA will bring with it its own copies of these inline header functions, and the linker gets to choose whichever one it wants!  Not fun if the linker chooses the avx2 version and uses it with everything
	# Thankfully, most linkers don't choose at random.  When presented with a bunch of .o files, most linkers seem to choose the first implementation they see, so make sure you order these from oldest to newest
	# Note: ld64 (macOS's linker) does not act the same way when presented with .a files, unless linked with `-force_load` (cmake WHOLE_ARCHIVE).
	set(is_first_isa "1")
	foreach(isa "sse4" "avx" "avx2")
		add_library(core_test_${isa} STATIC ${multi_isa_sources})
		target_link_libraries(core_test_${isa} PRIVATE PCSX2_FLAGS gtest)
		target_compile_definitions(core_test_${isa} PRIVATE MULTI_ISA_UNSHARED_COMPILATION=isa_${isa} MULTI_ISA_IS_FIRST=${is_first_isa} ${pcsx2_defs_${isa}})
		target_compile_options(core_test_${isa} PRIVATE ${compile_options_${isa}})
		if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.24)
			target_link_libraries(core_test PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,core_test_${isa}>)
		elseif(APPLE)
			message(FATAL_ERROR "MacOS builds with DISABLE_ADVANCE_SIMD=ON require CMake 3.24")
		else()
			target_link_libraries(core_test PRIVATE core_test_${isa})
		endif()
		set(is_first_isa "0")
	endforeach()
else()
	target_sources(core_test PRIVATE ${multi_isa_sources})
endif()