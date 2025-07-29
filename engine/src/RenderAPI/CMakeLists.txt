target_sources(Engine PRIVATE
	"CommandListManager.cpp"
	"CommandQueue.cpp"
	"Device.cpp"
	"Fence.cpp"
	"ImGui.cpp"
	"MemoryAliasing.cpp"
	"PipelineState.cpp"
	"Shader.cpp"
	"ShaderBindingTableBuilder.cpp"
	"StateObjectBuilder.cpp"
	"SwapChain.cpp"
)
add_subdirectory("Buffer")
add_subdirectory("Descriptor")