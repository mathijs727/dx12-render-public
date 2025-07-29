cmake_policy(PUSH)
if (${CMAKE_VERSION} GREATER_EQUAL "3.24")
	cmake_policy(SET CMP0135 NEW)
endif()
include(FetchContent)
FetchContent_Declare(
	dxc_compiler
	URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2212.1/dxc_2023_03_01.zip"
	URL_HASH SHA256=e4e8cb7326ff7e8a791acda6dfb0cb78cc96309098bfdb0ab1e465dc29162422
)
cmake_policy(POP)

if (NOT dxc_compiler_POPULATED)
	FetchContent_MakeAvailable(dxc_compiler)
endif()

set(DXC_LIB "${dxc_compiler_SOURCE_DIR}/lib/x64/dxcompiler.lib")
set(DXC_DLL "${dxc_compiler_SOURCE_DIR}/bin/x64/dxcompiler.dll")
set(DXC_EXE "${dxc_compiler_SOURCE_DIR}/bin/x64/dxc.exe")

add_library(ShaderCompiler SHARED IMPORTED)
target_include_directories(ShaderCompiler INTERFACE "${dxc_compiler_SOURCE_DIR}/inc/")
set_target_properties(ShaderCompiler  PROPERTIES
	IMPORTED_IMPLIB ${DXC_LIB}
	IMPORTED_LOCATION ${DXC_DLL})
