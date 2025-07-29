#include "common.hlsl"
#include "ShaderInputs/inputgroups/RayTraceLocalLayout/RTMesh.hlsl"
#include "ShaderInputs/inputgroups/RayTraceLocalLayout/SinglePBRMaterial.hlsl"
#include "ShaderInputs/inputlayouts/RayTraceLocalLayout.hlsl"

[shader("closesthit")] void pbrClosestHit(inout HitInfo payload, BuiltInTriangleIntersectionAttributes attrib) {
    const StructuredBuffer<uint32_t> indices = g_rTMesh.getIndices();
    const StructuredBuffer<Vertex> vertices = g_rTMesh.getVertices();
    const Texture2D baseColorTexture = g_singlePBRMaterial.getBaseColorTexture();

    const uint32_t baseIndex = 3 * PrimitiveIndex();
    const Vertex v0 = vertices[indices[baseIndex + 0]];
    const Vertex v1 = vertices[indices[baseIndex + 1]];
    const Vertex v2 = vertices[indices[baseIndex + 2]];

    const float2 texCoord = v0.texCoord * (1.0f - attrib.barycentrics.x - attrib.barycentrics.y) + attrib.barycentrics.x * v1.texCoord + attrib.barycentrics.y * v2.texCoord;
    float4 diffuseColor = baseColorTexture.SampleLevel(g_materialSampler, texCoord, 0);
    // TODO: ignore hits in the AnyHit shader.

    payload.colorAndDistance = float4(diffuseColor.xyz, RayTCurrent());
}

[shader("anyhit")] void pbrAnyHit(inout HitInfo payload, BuiltInTriangleIntersectionAttributes attrib) {
    const StructuredBuffer<uint32_t> indices = g_rTMesh.getIndices();
    const StructuredBuffer<Vertex> vertices = g_rTMesh.getVertices();
    const Texture2D baseColorTexture = g_singlePBRMaterial.getBaseColorTexture();

    const uint32_t baseIndex = 3 * PrimitiveIndex();
    const Vertex v0 = vertices[indices[baseIndex + 0]];
    const Vertex v1 = vertices[indices[baseIndex + 1]];
    const Vertex v2 = vertices[indices[baseIndex + 2]];

    const float2 texCoord = v0.texCoord * (1.0f - attrib.barycentrics.x - attrib.barycentrics.y) + attrib.barycentrics.x * v1.texCoord + attrib.barycentrics.y * v2.texCoord;
    const float alpha = baseColorTexture.SampleLevel(g_materialSampler, texCoord, 0).w;
    if (alpha < 1.0)
        IgnoreHit();
}