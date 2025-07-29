#include "vs_output.hlsl"
#include "vertex.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/StaticMeshVertex.hlsl"

VS_OUTPUT main(const VS_INPUT v, uint32_t vertexID : SV_VertexID)
{
    VS_OUTPUT ret;
    ret.pixelPosition = mul(g_staticMeshVertex.getModelViewProjectionMatrix(), float4(v.position, 1.0f));
    ret.worldPosition = mul(g_staticMeshVertex.getModelMatrix(), float4(v.position, 1.0f)).xyz;
    ret.viewSpacePosition = mul(g_staticMeshVertex.getModelViewMatrix(), float4(v.position, 1.0f)).xyz;
    ret.normal = normalize(mul(g_staticMeshVertex.getModelNormalMatrix(), v.normal));
    ret.texCoord = v.texCoord;
    return ret;
}
