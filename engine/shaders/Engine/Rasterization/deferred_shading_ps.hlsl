#include "ShaderInputs/inputgroups/DefaultLayout/DeferredShading.hlsl"
#include "Engine/Util/full_screen_vertex.hlsl"
#include "Engine/Materials/gltf_brdf.hlsl"
#include "Engine/Shared/surface_interaction.hlsl"
#include "Engine/Lights/directional_light.hlsl"

float4 main(FULL_SCREEN_VS_OUTPUT vertex)
    : SV_TARGET
{
    DeferredShading passInputs = g_deferredShading;
    const int3 pixel = int3(vertex.position.xy, 0);
    const float4 position_metallic = passInputs.getPosition_metallic().Load(pixel);
    const float4 normal_alpha= passInputs.getNormal_alpha().Load(pixel);
    const float3 baseColor = passInputs.getBaseColor().Load(pixel).xyz;
    const float sunVisibility = passInputs.getSunVisibility().Load(pixel);

    SurfaceInteraction si;
    si.p = position_metallic.xyz;
    si.n = normalize(normal_alpha.xyz);
    si.wo = normalize(passInputs.getCameraPosition() - si.p);

    GLTF_BRDF brdf;
    brdf.baseColor = baseColor;
    brdf.metallic = position_metallic.w;
    brdf.alpha = normal_alpha.w;

    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    // Sun light.
    {
        const DirectionalLight sun = passInputs.getSun();
        LightSample lightSample = light_sample(sun);
        //lightSample.luminance *= (0.1f + 0.9f * sunVisibility);
        lightSample.luminance *= sunVisibility;
        if (!isDummyLightSample(lightSample)) {
            const float3 f = brdf_f(brdf, si, lightSample.wi);
            Lo += lightSample.luminance * f / lightSample.pdf;
        }
    }
    return float4(Lo, 1.0f);
}