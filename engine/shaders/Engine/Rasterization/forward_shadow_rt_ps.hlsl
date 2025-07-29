#include "ShaderInputs/inputgroups/DefaultLayout/ForwardShadowRT.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/SinglePbrMaterial.hlsl"
#include "ShaderInputs/inputlayouts/DefaultLayout.hlsl"

// NVIDIA Real-Time Denoiser
#include "NRDEncoding.hlsli"
#include "NRD.hlsli"

#include "Engine/Lights/directional_light.hlsl"
//#include "Engine/Lights/point_light.hlsl"
#include "Engine/Materials/gltf_brdf.hlsl"
#include "Engine/Shared/surface_interaction.hlsl"
#include "Engine/Util/random.hlsl"
#include "Engine/Shared/vs_output.hlsl"

cbuffer DrawConstants : ROOT_CONSTANT_DRAWID {
    uint32_t drawID;
};

template <typename T>
SurfaceInteraction constructSI(const T fragment)
{
    SurfaceInteraction si;
    si.p = fragment.worldPosition;
    si.wo = normalize(g_forwardShadowRT.getCameraPosition() - fragment.worldPosition);
    si.n = si.gn = normalize(fragment.normal);
    //if (dot(g_forward.getCameraPosition() - si.p, si.n) < 0)
    //    si.n = -si.n;
    si.uv = fragment.texCoord;
    return si;
}

#define epsilon 0.0001f

bool testVisibility(in RaytracingAccelerationStructure accel, in RayDesc ray) {
    // Instantiate ray query object. Template parameter allows driver to generate a specialized implementation.
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
    //RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;

    // Set up a trace.  No work is done yet.
    q.TraceRayInline(accel, RAY_FLAG_NONE, 0xFF, ray);

    // Proceed() below is where behind-the-scenes traversal happens, including the heaviest of any driver inlined code.
    // In this simplest of scenarios, Proceed() only needs to be called once rather than a loop: based on the template
    // specialization above, traversal completion is guaranteed.
    q.Proceed();

    //hitDistance = ray.TMax;
    return q.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
}

float4 main(const VS_OUTPUT fragment) : SV_Target0
{
    ForwardShadowRT passInputs = g_forwardShadowRT;
    RNG rng = createRandomNumberGeneratorFromFragment(passInputs.getRandomSeed(), fragment.pixelPosition.xy);

    const SurfaceInteraction si = constructSI(fragment);
    GLTF_BRDF brdf;
    if (!brdf_construct(si, g_singlePBRMaterial, brdf))
        discard;

    DirectionalLight sun = passInputs.getSun();
    //light.direction += rngNextFloat3(rng) * 0.01f;
    RayDesc sunVisibilityRay;
    const LightSample lightSample = light_sample(sun, si, sunVisibilityRay);

    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    if (testVisibility(passInputs.getAccelerationStructure(), sunVisibilityRay)) {
        const float3 f = brdf_f(brdf, si, lightSample.wi);
        Lo =  lightSample.luminance * f / lightSample.pdf;
    }
    return float4(Lo, 1.0f);
}