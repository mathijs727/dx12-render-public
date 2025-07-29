#pragma once
#include <memory_resource>

namespace ShaderInputs {
struct Material;
struct Vertex;
}

namespace Render {

enum class TextureFileType {
    PNG,
    JPG,
    DDS,
    KTX2,
    HDR,
    OpenEXR,
    Uknown
};

struct Camera;
class DebugBufferReader;
struct TextureCPU;
struct Texture;
struct SubMesh;
struct Material;
struct Mesh;
class GPUFrameProfiler;
struct RenderContext;
template <typename T>
struct Transformable;
struct Scene;
class ShaderCache;
class ShaderHotReload;
struct PointLight;

}
