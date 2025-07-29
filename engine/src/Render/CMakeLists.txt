target_sources(Engine PRIVATE
	"Camera.cpp"
	"Debug.cpp"
	"GPUProfiler.cpp"
	"Light.cpp"
	"Mesh.cpp"
	"RenderContext.cpp"
	"Scene.cpp"
	"ShaderHotReload.cpp"
	"Texture.cpp"
	"VkFormat.h"
)

add_subdirectory("FrameGraph")
#add_subdirectory("ImageIO")
add_subdirectory("RenderPasses")