add_subdirectory("FrameGraph")
add_subdirectory("RenderPasses")

target_sources(Engine PRIVATE
	"Camera.h"
	"Debug.h"
	"ForwardDeclares.h"
	"GPUProfiler.h"
	"Light.h"
	"Mesh.h"
	
	"RenderContext.h"
	"Scene.h"
	
	"ShaderHotReload.h"
	"Texture.h"
)
