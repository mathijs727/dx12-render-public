add_executable(EngineTest
	"src/Main.cpp"
	"src/Core/Bounds.cpp"
	"src/Memory/FixedSizePoolAllocator.cpp"
	"src/Memory/LinearAllocator.cpp"
	"src/Memory/Memory.cpp"
	"src/Memory/UnintrusiveBuddyAllocator.cpp"
	"src/Util/Align.cpp"
	"src/Util/BinaryReaderWriter.cpp"
	"src/Util/CompileTimeStringMap.cpp"
	"src/Util/ErrorHandling.cpp"
	"src/Util/IsOfType.cpp"
	"src/Util/Math.cpp"
	"src/Render/GPU.cpp"
	"src/Render/GPUPrintf.cpp"
	"src/Render/GPURandom.cpp"
	"src/Render/GPURender.cpp"
	"src/Render/RenderContext.cpp"
	"src/Render/Texture.cpp"
)
target_include_directories(EngineTest PRIVATE "src")
target_link_libraries(EngineTest PUBLIC
	project_options
	project_warnings
	Catch2::Catch2
	Engine
)
CATCH_DISCOVER_TESTS(EngineTest)
engine_compile_all_hlsl(EngineTest)

# Copy data files to the test output directory
add_custom_command(TARGET EngineTest POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_LIST_DIR}/assets" "$<TARGET_FILE_DIR:EngineTest>/assets")

# Generate C++ and HLSL code from the shader input files
set(SHADER_INPUT_FILE "${CMAKE_CURRENT_LIST_DIR}/shaders/inputs.si")
add_custom_target(
	EngineTestGenerateShaderInputs
	COMMAND ShaderInputCompiler ${SHADER_INPUT_FILE}
	DEPENDS ${SHADER_INPUT_FILE}
)
add_dependencies(EngineTest EngineTestGenerateShaderInputs)

# Compile HLSL files using the DirectX ShEngineTestader Compiler (DXC).
function(engine_test_compile_all_hlsl)
	set(SHADER_INPUT_FOLDER "${CMAKE_CURRENT_LIST_DIR}/shaders/")
	set(SHADER_OUTPUT_FOLDER "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
	set(ENGINE_SHADER_FOLDER "${CMAKE_CURRENT_LIST_DIR}/../shaders")

	add_custom_target(EngineTestCompileShaders)
	add_dependencies(EngineTestCompileShaders EngineTestGenerateShaderInputs)
	add_dependencies(EngineTest EngineTestCompileShaders)

	set(DXC_OPTIONS "")
	list(APPEND DXC_OPTIONS "-I ${SHADER_INPUT_FOLDER}")
	list(APPEND DXC_OPTIONS "-I ${ENGINE_SHADER_FOLDER}")
	macro(_compile RELATIVE_SHADER_FILE SHADER_TYPE)
		compile_hlsl_file_relative(EngineTest ${SHADER_INPUT_FOLDER} ${SHADER_OUTPUT_FOLDER} ${RELATIVE_SHADER_FILE} ${SHADER_TYPE} "${DXC_OPTIONS}")
	endmacro()
	
	_compile("Test/test_random_uint_cs" "cs")
	_compile("Test/test_random_float_cs" "cs")
	_compile("Test/test_printf_cs" "cs")
endfunction()

engine_test_compile_all_hlsl()
