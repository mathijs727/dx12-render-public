add_library(Toolbox INTERFACE)
#add_subdirectory("src")

target_include_directories(Toolbox INTERFACE "include")

target_link_libraries(Toolbox INTERFACE
	project_options
	project_warnings
	EASTL
	fmt::fmt
	spdlog::spdlog
	magic_enum::magic_enum
)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_definitions(Toolbox INTERFACE "NOMINMAX")
	# Surpess non-dll interface class warnings from fmt
	target_compile_options(Toolbox INTERFACE "/wd4275")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(Toolbox INTERFACE "-mpopcnt")
endif()

if (DX12_RENDER_ENABLE_TESTS)
	add_subdirectory("tests")
endif()