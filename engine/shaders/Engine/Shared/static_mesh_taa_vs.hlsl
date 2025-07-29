#include "vs_output.hlsl"
#include "vertex.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/StaticMeshTAAVertex.hlsl"

VS_OUTPUT_TAA main(const VS_INPUT v)
{
    VS_OUTPUT_TAA ret;
    ret.jitteredPixelPosition = mul(g_staticMeshTAAVertex.getJitteredModelViewProjectionMatrix(), float4(v.position, 1.0f));

    ret.worldPosition = mul(g_staticMeshTAAVertex.getModelMatrix(), float4(v.position, 1.0f)).xyz;
    ret.viewSpacePosition = mul(g_staticMeshTAAVertex.getModelViewMatrix(), float4(v.position, 1.0f)).xyz;
    ret.normal = normalize(mul(g_staticMeshTAAVertex.getModelNormalMatrix(), v.normal));
    ret.texCoord = v.texCoord;

    ret.screenPosition = mul(g_staticMeshTAAVertex.getModelViewProjectionMatrix(), float4(v.position, 1.0f));
    ret.lastFrameScreenPosition = mul(g_staticMeshTAAVertex.getLastFrameModelViewProjectionMatrix(), float4(v.position, 1.0f));
    return ret;
}
