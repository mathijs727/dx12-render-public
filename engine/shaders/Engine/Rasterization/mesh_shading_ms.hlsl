#include "Engine/Util/maths.hlsl"
#include "Engine/Shared/vertex.hlsl"
#include "Engine/Shared/vs_output.hlsl"
#include "Engine/Util/full_screen_vertex.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/MeshShading.hlsl"
#include "ShaderInputs/structs/Meshlet.hlsl"
#include "ShaderInputs/constants.hlsl"
#include "Engine/Util/random.hlsl"

struct VERTEX_DATA {
    float4 position : SV_Position;
    float3 normal : NORMAL0;
    float3 color : COLOR0;
};

VERTEX_DATA convertVertex(in Vertex v, in MeshShading inputs, float3 color) {
    VERTEX_DATA ret;
    ret.position = mul(inputs.getModelViewProjectionMatrix(), float4(v.pos, 1.0f));
    ret.normal = normalize(mul(inputs.getModelNormalMatrix(), v.normal));
    ret.color = color;
    return ret;
}

// TODO: generate these using the ShaderInputCompiler
#define MESH_SHADING_WORK_GROUP_SIZE 64

[NumThreads(MESH_SHADING_WORK_GROUP_SIZE, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint threadIdxInGroup : SV_GroupThreadID,
    uint groupIdx : SV_GroupID,
    out vertices VERTEX_DATA verts[MESHLET_MAX_VERTICES],
    out indices uint3 tris[MESHLET_MAX_PRIMITIVES]
)
{
    const Meshlet meshlet = g_meshShading.getMeshlets()[g_meshShading.getMeshletStart() + groupIdx];
    SetMeshOutputCounts(meshlet.numVertices, meshlet.numPrimitives);
    
    RNG rng = createRandomNumberGenerator(79821479821, groupIdx);
    const float3 color = rng.generateFloat3();
    
    for (uint32_t vertexBase = 0; vertexBase < meshlet.numVertices; vertexBase += MESH_SHADING_WORK_GROUP_SIZE) {
        const uint32_t i = vertexBase + threadIdxInGroup;
        if (i < meshlet.numVertices)
            verts[i] = convertVertex(g_meshShading.getVertices()[meshlet.vertices[i]], g_meshShading, color);
    }

    for (uint32_t indexBase = 0; indexBase < meshlet.numPrimitives; indexBase += MESH_SHADING_WORK_GROUP_SIZE) {
        const uint32_t i = indexBase + threadIdxInGroup;
        if (i < meshlet.numPrimitives) {
            const uint32_t encodedPrimitive = meshlet.primitives[i];
            tris[i] = uint3(encodedPrimitive & 0xFF, (encodedPrimitive >> 8) & 0xFF, (encodedPrimitive >> 16) & 0xFF);
        }
    }
}