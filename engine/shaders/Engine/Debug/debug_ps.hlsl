#include "ShaderInputs/inputgroups/DefaultLayout/RasterDebug.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/SinglePBRMaterial.hlsl"
#include "ShaderInputs/inputlayouts/DefaultLayout.hlsl"
#include "Engine/Shared/vs_output.hlsl"
#include "Engine/Debug/visual_debug.hlsl"

float4 main(const VS_OUTPUT fragment) : SV_TARGET
{
    const Texture2D baseColorTexture = g_singlePBRMaterial.getBaseColorTexture();
    float4 baseColorTexSample = baseColorTexture.Sample(g_materialSampler, fragment.texCoord);
    if (baseColorTexSample.w == 0)
        discard;
    
    const int2 pixelPos = int2(fragment.pixelPosition.xy);
    const int2 debugPixel = g_rasterDebug.getDebugPixel();
    if (all(pixelPos == debugPixel)) {
        const float3 from = fragment.worldPosition;
        const float3 to = fragment.worldPosition + fragment.normal;
        const float3 color = 0.5f * fragment.normal + 0.5f;
        visualDebugDrawArrow(g_rasterDebug.getVisualDebug(), from, to, color );
    }
    
    const float3 baseColor = baseColorTexSample.rgb * g_singlePBRMaterial.getMaterial().baseColor;
    //return float4(abs(fragment.worldPosition) / 10.0f, 1);
    float3 outColor = baseColor;
    //soutColor = outColor * abs(fragment.normal);
    return float4(outColor, 1.0f);
}
