#include "ShaderInputs/inputgroups/DefaultLayout/Forward.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/SinglePBRMaterial.hlsl"
#include "ShaderInputs/inputlayouts/DefaultLayout.hlsl"

#include "Engine/Lights/directional_light.hlsl"
#include "Engine/Materials/gltf_brdf.hlsl"
#include "Engine/Shared/surface_interaction.hlsl"
#include "Engine/Util/random.hlsl"
#include "Engine/Shared/vs_output.hlsl"

template <typename T>
SurfaceInteraction constructSI(const T fragment)
{
    SurfaceInteraction si;
    si.p = fragment.worldPosition;
    si.wo = normalize(g_forward.getCameraPosition() - fragment.worldPosition);
    si.n = normalize(fragment.normal);
    //if (dot(g_forward.getCameraPosition() - si.p, si.n) < 0)
    //    si.n = -si.n;
    si.uv = fragment.texCoord;
    return si;
}

#if SUPPORT_TAA
struct PS_OUTPUT
{
    float4 color: SV_Target0;
    float2 velocity: SV_Target1;
};

PS_OUTPUT main(const VS_OUTPUT_TAA fragment)
{
#else
float4 main(const VS_OUTPUT fragment) : SV_Target
{
#endif
    Forward passInputs = g_forward;

    const SurfaceInteraction si = constructSI(fragment);

    GLTF_BRDF brdf;
    if (!brdf_construct(si, g_singlePBRMaterial, brdf))
        discard;

    //RNG rng = createRandomNumberGenerator(passInputs.getRandomSeed(), fragment.screenPosition.xy / passInputs.getViewportSize());
    float3 Lo = float3(0.0f, 0.0f, 0.0f);

    // Directional lights
    {
        const DirectionalLight sun = passInputs.getSun();
        const LightSample lightSample = light_sample(sun);
        if (!isDummyLightSample(lightSample)) {
            const float3 f = brdf_f(brdf, si, lightSample.wi);
            Lo += lightSample.luminance * f / lightSample.pdf;
        }
    }

#if SUPPORT_TAA
    const float2 screenPosition = fragment.screenPosition.xy / fragment.screenPosition.w;
    const float2 lastFrameScreenPosition = fragment.lastFrameScreenPosition.xy / fragment.lastFrameScreenPosition.w;
    PS_OUTPUT output;
    output.color = float4(Lo, 1.0f);
    output.velocity = screenPosition - lastFrameScreenPosition;
    return output;
#else
    return float4(Lo, 1.0f);
#endif
}