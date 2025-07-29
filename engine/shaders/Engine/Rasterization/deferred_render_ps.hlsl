#include "ShaderInputs/inputgroups/DefaultLayout/SinglePBRMaterial.hlsl"
#include "ShaderInputs/inputlayouts/DefaultLayout.hlsl"
#include "Engine/Materials/gltf_brdf.hlsl"
#include "Engine/Shared/surface_interaction.hlsl"
#include "Engine/Shared/vs_output.hlsl"

SurfaceInteraction constructSI(const VS_OUTPUT fragment)
{
    SurfaceInteraction si;
    si.p = fragment.worldPosition;
    //si.wo = normalize(g_forward.getCameraPosition() - fragment.worldPosition);
    si.n = normalize(fragment.normal);
    si.uv = fragment.texCoord;
    return si;
}

struct PS_OUTPUT
{
    float4 position_metallic: SV_Target0;
    float4 normal_alpha : SV_Target1;
    float4 baseColor: SV_Target2;
};
PS_OUTPUT main(const VS_OUTPUT fragment)
{
    const SurfaceInteraction si = constructSI(fragment);
    GLTF_BRDF brdf;
    if (!brdf_construct(si, g_singlePBRMaterial, brdf))
        discard;
    
    PS_OUTPUT output;
    output.position_metallic = float4(fragment.worldPosition, brdf.metallic);
    output.normal_alpha = float4(fragment.normal, brdf.alpha);
    output.baseColor = float4(brdf.baseColor, 1);
    return output;
}