cmake_minimum_required(VERSION 3.17)
project(DX12RenderProject VERSION 0.0)

include(cmake/StandardProjectSettings.cmake)
# Contains the code necessary to enable HotReload in Visual Studio 2022
include(cmake/Windows.cmake)

# Options
option(DX12_RENDER_ENABLE_VALIDATION_LAYER BOOL FALSE)
option(DX12_RENDER_ENABLE_TESTS BOOL TRUE)
option(DX12_RENDER_ENABLE_IWYU BOOL FALSE)

# Link this 'library' to set the c++ standard / compile-time options requested
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)
# Sanitizer options if supported by compiler
include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)
# Standard compiler warnings
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

# enable doxygen
#include(cmake/Doxygen.cmake)
#enable_doxygen()

# allow for static analysis options
include(cmake/StaticAnalyzers.cmake)

if (DX12_RENDER_ENABLE_IWYU)
	message("Enabling include-what-you-use")
	find_program(IWYU_PATH NAMES include-what-you-use iwyu REQUIRED)
	set(CMAKE_C_INCLUDE_WHAT_YOU_USE ${IWYU_PATH} -Xiwyu "--mapping_file=${CMAKE_CURRENT_LIST_DIR}/msvc.imp")
	set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH} -Xiwyu "--mapping_file=${CMAKE_CURRENT_LIST_DIR}/msvc.imp")
endif()

# Packages are installed automatically using vcpkg manifest.
find_package(Catch2 CONFIG REQUIRED)
find_package(cppitertools CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(D3D12MemoryAllocator CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)
find_package(directxmesh CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(EABase CONFIG REQUIRED)
find_package(EASTL CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(lexy CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(mio CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
# Just stand-alone header file with no CMake attached
find_path(D3DX12_INCLUDE_DIRS "d3dx12.h")
add_library(D3DX12 INTERFACE)
target_include_directories(D3DX12 INTERFACE ${D3DX12_INCLUDE_DIRS})

include("cmake/DXCCompiler.cmake")

# https://stackoverflow.com/questions/41695993/ignore-warnings-in-external-modules-when-using-cmake
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
cmake_policy(SET CMP0077 NEW)

set(DXC_PATH ${DXC_EXE})
set(NRD_STATIC_LIBRARY ON)
set(NRD_EMBEDS_SPIRV_SHADERS OFF)
set(NRD_EMBEDS_DXIL_SHADERS ON)
set(NRD_EMBEDS_DXBC_SHADERS OFF)
include(FetchContent)
FetchContent_Declare(
	nrd
	GIT_REPOSITORY https://github.com/NVIDIAGameWorks/RayTracingDenoiser.git
	GIT_TAG v4.15.0
	#PATCH_COMMAND git apply "${CMAKE_CURRENT_SOURCE_DIR}/third_party/RayTracingDenoiser.patch"
	#UPDATE_DISCONNECTED 1 # Only apply patch once
)
FetchContent_MakeAvailable(nrd)

enable_testing()
include(CTest)
include(Catch)

add_subdirectory("toolbox")
add_subdirectory("shader_input_compiler")
add_subdirectory("engine")
add_subdirectory("editor")
add_subdirectory("gltf_optimizer")
