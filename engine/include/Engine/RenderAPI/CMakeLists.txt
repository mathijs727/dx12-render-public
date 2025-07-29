add_subdirectory("Buffer")
add_subdirectory("Descriptor")
add_subdirectory("Internal")

target_sources(Engine PRIVATE
	"CommandListManager.h"
	"CommandQueue.h"
	"Device.h"
	"Fence.h"
	"ForwardDeclares.h"
	"ImGui.h"
	
	"MemoryAliasing.h"
	"PipelineState.h"
	"RenderAPI.h"
	"Shader.h"
	"ShaderBindingTableBuilder.h"
	"ShaderInput.h"
	"StateObjectBuilder.h"
	"SwapChain.h"
)
