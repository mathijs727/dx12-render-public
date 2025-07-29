add_library(Engine STATIC)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/src/")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/include/")
target_include_directories(Engine PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include/")
#enable_edit_and_continue(Engine)

if (WIN32)
	target_compile_definitions(Engine PUBLIC "NOMINMAX")
	# fmtlib error message about inheriting from std::runtime_error in DLL class
	target_compile_options(Engine PRIVATE "/wd4275")
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_definitions(Engine PUBLIC "MSVC_COMPILER")
endif()

# TODO(Mathijs): make sure this still holds when adding a new back-end (such as OpenGL or Vulkan).
# Sticking to depth 0 to 1 everywhere is probably the best option since it is the superior solution.
target_compile_options(Engine PUBLIC "-DGLM_FORCE_DEPTH_ZERO_TO_ONE=1" "-DGLM_ENABLE_EXPERIMENTAL=1" "-DIMGUI_DEFINE_MATH_OPERATORS=1")

# Speed up compilation with precompiled headers.
target_precompile_headers(Engine
	PRIVATE
		"<filesystem>"
		"<chrono>"
		"<windows.h>"
		"<Engine/RenderAPI/Internal/D3D12Includes.h>"
		"<Engine/RenderAPI/RenderAPI.h>"
		"<tbx/hashmap_helper.h>"
	)

find_library(D3D12_LIB "d3d12.lib" REQUIRED)
find_library(DXGI_LIB "dxgi.lib" REQUIRED)
find_library(DXGUID_LIB "dxguid.lib" REQUIRED) # https://github.com/microsoft/DirectX-Graphics-Samples/issues/567
set(D3D12_LIBRARIES ${D3D12_LIB} ${DXGI_LIB} ${DXGUID_LIB})
target_link_libraries(Engine
	PUBLIC
		Toolbox
		cppitertools::cppitertools
		Microsoft::DirectX-Guids Microsoft::DirectX-Headers D3DX12
		Microsoft::DirectXMesh
		Microsoft::DirectXTex
		EABase EASTL
		fmt::fmt spdlog::spdlog
		glm::glm
		KTX::ktx
		imgui::imgui
		magic_enum::magic_enum
		mio::mio
		nfd::nfd
		GPUOpen::D3D12MemoryAllocator
		NRD
		ShaderCompiler
		${D3D12_LIBRARIES}
	PRIVATE
		nlohmann_json::nlohmann_json
		project_options
		project_warnings)

if (DX12_RENDER_ENABLE_VALIDATION_LAYER)
	message("D3D12 validation layers enabled")
	target_compile_definitions(Engine PRIVATE "-DD3D12_ENABLE_VALIDATION")
endif()

target_compile_definitions(Engine PRIVATE "-DCMAKE_EXECUTABLE=\"${CMAKE_COMMAND}\"")

# Generate the C++ code from the shader input files.
set(SHADER_INPUT_FILE "${CMAKE_CURRENT_LIST_DIR}/shaders/inputs.si")
add_custom_target(
	EngineGenerateShaderInputs
	COMMAND ShaderInputCompiler ${SHADER_INPUT_FILE}
	DEPENDS ${SHADER_INPUT_FILE}
)
add_dependencies(Engine EngineGenerateShaderInputs NRDShaders)

# Compile HLSL shaders using the DirectX Shader Compiler (DXC).
function(compile_hlsl_file CMAKE_TARGET INPUT_FILE_PATH OUTPUT_FILE_PATH SHADER_TYPE DXC_OPTIONS)
	set(ENTRY_POINT_OPTION "-E" "main")	
	set(SHADER_PROFILE "${SHADER_TYPE}_6_6")
	if (SHADER_TYPE MATCHES "lib")
		set(ENTRY_POINT_OPTION "")
	endif()

	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		list(APPEND DXC_OPTIONS "/Zi")
	endif()

	get_filename_component(OUTPUT_FOLDER "${OUTPUT_FILE_PATH}" DIRECTORY)
	add_custom_command(
		TARGET ${CMAKE_TARGET}
		PRE_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_FOLDER}"
		COMMAND ${DXC_EXE} ${ENTRY_POINT_OPTION} ${DXC_OPTIONS} "-Fo" ${OUTPUT_FILE_PATH} "-T" ${SHADER_PROFILE} "-HV" "2021" ${INPUT_FILE_PATH}
	)
endfunction()

function(compile_hlsl_file_relative CMAKE_TARGET INPUT_BASE_FOLDER OUTPUT_BASE_FOLDER RELATIVE_SHADER_FILE SHADER_TYPE DXC_OPTIONS)
	set(INPUT_FILE_PATH "${INPUT_BASE_FOLDER}/${RELATIVE_SHADER_FILE}.hlsl")
	set(OUTPUT_FILE_PATH "${OUTPUT_BASE_FOLDER}/${RELATIVE_SHADER_FILE}.dxil")
	compile_hlsl_file(${CMAKE_TARGET} ${INPUT_FILE_PATH} ${OUTPUT_FILE_PATH} ${SHADER_TYPE} "${DXC_OPTIONS}")
endfunction()

function(compile_hlsl_file_relative_variant CMAKE_TARGET INPUT_BASE_FOLDER OUTPUT_BASE_FOLDER RELATIVE_SHADER_FILE OUTPUT_FILE_NAME SHADER_TYPE DXC_OPTIONS)
	get_filename_component(RELATIVE_SHADER_DIRECTORY "${RELATIVE_SHADER_FILE}" DIRECTORY)
	set(INPUT_FILE_PATH "${INPUT_BASE_FOLDER}/${RELATIVE_SHADER_FILE}.hlsl")
	set(OUTPUT_FILE_PATH "${OUTPUT_BASE_FOLDER}/${RELATIVE_SHADER_DIRECTORY}/${OUTPUT_FILE_NAME}.dxil")
	compile_hlsl_file(${CMAKE_TARGET} ${INPUT_FILE_PATH} ${OUTPUT_FILE_PATH} ${SHADER_TYPE} "${DXC_OPTIONS}")
endfunction()


# This function compiles all HLSL shaders for the Engine.
function(engine_compile_all_hlsl EXECUTABLE_TARGET)
	set(SHADER_INPUT_FOLDER "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shaders/")
	set(SHADER_OUTPUT_FOLDER "${CMAKE_CURRENT_BINARY_DIR}/shaders/")

	set(COMPILE_SHADERS_TARGET_NAME "CompileEngineShaders_${EXECUTABLE_TARGET}")
	# CMake target to compile shaders
	add_custom_target(${COMPILE_SHADERS_TARGET_NAME})
	# Requires the HLSL shader input files to be generated first.
	add_dependencies(${COMPILE_SHADERS_TARGET_NAME} EngineGenerateShaderInputs NRDShaders)
	# Make target executable depend on the shader compilation target.
	add_dependencies(${EXECUTABLE_TARGET} ${COMPILE_SHADERS_TARGET_NAME})

	set(DXC_OPTIONS "")
	list(APPEND DXC_OPTIONS "-I ${SHADER_INPUT_FOLDER}")
	list(APPEND DXC_OPTIONS "-I ${nrd_SOURCE_DIR}/Shaders/Include/")
	#list(APPEND DXC_OPTIONS "-enable-16bit-types")
	macro(_compile RELATIVE_SHADER_FILE SHADER_TYPE)
		compile_hlsl_file_relative(${COMPILE_SHADERS_TARGET_NAME} ${SHADER_INPUT_FOLDER} ${SHADER_OUTPUT_FOLDER} ${RELATIVE_SHADER_FILE} ${SHADER_TYPE} "${DXC_OPTIONS}")
	endmacro()
	macro(_compile_variant RELATIVE_SHADER_FILE OUTPUT_FILE_NAME SHADER_TYPE EXTRA_DXC_OPTIONS)
		set(COMBINED_DXC_OPTIONS ${DXC_OPTIONS} ${EXTRA_DXC_OPTIONS})
		compile_hlsl_file_relative_variant(${COMPILE_SHADERS_TARGET_NAME} ${SHADER_INPUT_FOLDER} ${SHADER_OUTPUT_FOLDER} ${RELATIVE_SHADER_FILE} ${OUTPUT_FILE_NAME} ${SHADER_TYPE} "${COMBINED_DXC_OPTIONS}")
	endmacro()

	_compile("Engine/Debug/debug_random_cs" "cs")
	_compile("Engine/Debug/debug_ps" "ps")
	_compile("Engine/Debug/debug_rt_cs" "cs")
	_compile("Engine/Debug/visual_debug_vs" "vs")
	_compile("Engine/Debug/visual_debug_ps" "ps")
	_compile("Engine/Debug/rt/ray_gen" "lib")
	_compile("Engine/Debug/rt/miss" "lib")
	_compile("Engine/Debug/rt/hit_group" "lib")
	_compile("Engine/Shared/static_mesh_vs" "vs")
	_compile("Engine/Shared/static_mesh_taa_vs" "vs")
	_compile_variant("Engine/Rasterization/forward_ps" "forward_ps" "ps" "-DSUPPORT_TAA=0")
	_compile_variant("Engine/Rasterization/forward_ps" "forward_taa_ps" "ps" "-DSUPPORT_TAA=1")
	_compile("Engine/Rasterization/forward_shadow_rt_ps" "ps")
	_compile("Engine/Rasterization/deferred_render_ps" "ps")
	_compile("Engine/Rasterization/deferred_shading_ps" "ps")
	_compile("Engine/Rasterization/sun_visibility_rt_cs" "cs")
	_compile("Engine/Rasterization/mesh_shading_bindless_as" "as")
	_compile("Engine/Rasterization/mesh_shading_bindless_ms" "ms")
	_compile("Engine/Rasterization/mesh_shading_ms" "ms")
	_compile("Engine/Rasterization/mesh_shading_ps" "ps")
	_compile("Engine/Rasterization/visibility_buffer_render_ps" "ps")
	_compile("Engine/Rasterization/visibility_to_gbuffer_ps" "ps")
	_compile_variant("Engine/RayTracing/path_tracing" "path_tracing_lib" "lib"  "-DDRAW_DEBUG_RAYS=0")
	_compile_variant("Engine/RayTracing/path_tracing" "path_tracing_debug_lib" "lib" "-DDRAW_DEBUG_RAYS=1")
	_compile("Engine/Util/full_screen_vs" "vs")
	_compile("Engine/Util/copy_ps" "ps")
	_compile("Engine/Util/copy_channels_ps" "ps")
	_compile("Engine/PostProcessing/color_correction_ps" "ps")
	_compile("Engine/PostProcessing/nvidia_denoise_decode_cs" "cs")
	_compile("Engine/PostProcessing/taa_resolve_ps" "ps")
endfunction()


if (DX12_RENDER_ENABLE_TESTS)
	add_subdirectory("tests")
endif()
