add_executable(gltf_optimizer "src/main.cpp")
target_compile_features(gltf_optimizer PRIVATE cxx_std_20)
target_link_libraries(gltf_optimizer PRIVATE
	project_options
	project_warnings
	Engine
	CLI11::CLI11
	fmt::fmt spdlog::spdlog
	runtimeobject.lib
)