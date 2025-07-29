#include "Engine/Shared/vertex.hlsl"
#include "Engine/Shared/vs_output.hlsl"
#include "Engine/Util/full_screen_vertex.hlsl"
#include "Engine/Util/maths.hlsl"
#include "Engine/Util/random.hlsl"
#include "ShaderInputs/constants.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/BindlessScene.hlsl"
#include "ShaderInputs/structs/Meshlet.hlsl"
#include "mesh_shading.hlsl"

struct VERTEX_DATA {
    float4 position : SV_Position;
    float3 normal : NORMAL0;
    float3 color : COLOR0;
};

VERTEX_DATA convertVertex(in Vertex v, in Payload payload, float3 color)
{
    VERTEX_DATA ret;
    ret.position = mul(payload.mvpMatrix, float4(v.pos, 1.0f));
    ret.normal = normalize(mul(payload.normalMatrix, v.normal));
    ret.color = color;
    return ret;
}

// TODO: generate these using the ShaderInputCompiler
#define MESH_SHADING_WORK_GROUP_SIZE 64

[NumThreads(MESH_SHADING_WORK_GROUP_SIZE, 1, 1)]
    [OutputTopology("triangle")] void
    main(
        in uint threadIdxInGroup : SV_GroupThreadID,
        in uint groupIdx : SV_GroupID,
        in payload Payload payload,
        out vertices VERTEX_DATA verts[MESHLET_MAX_VERTICES],
        out indices uint3 tris[MESHLET_MAX_PRIMITIVES]
) {
        const Meshlet meshlet = g_bindlessScene.getMeshlets(payload.meshIdx)[groupIdx];
        SetMeshOutputCounts(meshlet.numVertices, meshlet.numPrimitives);

        RNG rng = createRandomNumberGenerator(79821479821, groupIdx);
        const float3 color = rng.generateFloat3();

        StructuredBuffer<Vertex> vertices = g_bindlessScene.getVertexBuffers(payload.meshIdx);
        for (uint32_t vertexBase = 0; vertexBase < meshlet.numVertices; vertexBase += MESH_SHADING_WORK_GROUP_SIZE) {
            const uint32_t i = vertexBase + threadIdxInGroup;
            if (i < meshlet.numVertices)
                verts[i] = convertVertex(vertices[meshlet.vertices[i]], payload, color);
        }

        for (uint32_t indexBase = 0; indexBase < meshlet.numPrimitives; indexBase += MESH_SHADING_WORK_GROUP_SIZE) {
            const uint32_t i = indexBase + threadIdxInGroup;
            if (i < meshlet.numPrimitives) {
                const uint32_t encodedPrimitive = meshlet.primitives[i];
                tris[i] = uint3(encodedPrimitive & 0xFF, (encodedPrimitive >> 8) & 0xFF, (encodedPrimitive >> 16) & 0xFF);
            }
        }
    }