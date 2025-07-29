add_executable(Editor WIN32 "src/Main.cpp")
target_compile_definitions(Editor PRIVATE ASSET_FOLDER="${PROJECT_SOURCE_DIR}/assets/")
target_compile_features(Editor PRIVATE cxx_std_20)
target_link_libraries(Editor PRIVATE
	project_options
	project_warnings
	Engine
	CLI11::CLI11
	cppitertools::cppitertools
	fmt::fmt spdlog::spdlog
	imgui::imgui
)

engine_compile_all_hlsl(Editor)
#enable_edit_and_continue(Editor)

target_compile_definitions(Editor PRIVATE "-DENGINE_SHADER_SOURCE_DIR=\"${CMAKE_CURRENT_LIST_DIR}/../engine/shaders/\"")
target_compile_definitions(Editor PRIVATE "-DBUILD_DIR=\"${CMAKE_BINARY_DIR}\"")