add_executable(ShaderInputCompiler
	"src/backends/dx12-render/ConstantBuffer.cpp"
	"src/backends/dx12-render/DescriptorTableAllocator.cpp"
	"src/backends/dx12-render/GenerateDeviceCode.cpp"
	"src/backends/dx12-render/GenerateHostCode.cpp"
	"src/backends/dx12-render/HLSLRegister.cpp"
	"src/backends/dx12-render/RegisterAllocation.cpp"
	"src/Main.cpp"
	"src/ParseTree.cpp"
	"src/StringManipulation.cpp"
 )
target_include_directories(ShaderInputCompiler PRIVATE "src")

target_link_libraries(ShaderInputCompiler PUBLIC
	project_options
	project_warnings
	Toolbox
	CLI11::CLI11
	cppitertools::cppitertools
	fmt::fmt spdlog::spdlog
	foonathan::lexy
	magic_enum::magic_enum
)

target_compile_definitions(ShaderInputCompiler PRIVATE "-DBASE_DIR=\"${CMAKE_CURRENT_LIST_DIR}/\"")
