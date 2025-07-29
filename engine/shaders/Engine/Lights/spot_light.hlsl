#ifndef __SPOTLIGHT_HLSL__
#define __SPOTLIGHT_HLSL__
#include "inputs/structs.hlsl"
#include "light.hlsl"
#include "shared/surface_interaction.hlsl"
#include "util/random.hlsl"

#if SHADOW_TYPE == 0
Texture2D shadowMap : register(t0, space3);
SamplerState sampler1 : register(s0, space3);
#endif

LightSample light_sample(const SpotLight light, const SurfaceInteraction si, const float2 u)
{
    // https://learnopengl.com/Lighting/Light-casters
    const float3 siToLight = light.position - si.p;
    const float theta = dot(normalize(siToLight), -light.direction);

    if ((1.0f - theta) < light.cutOff) {
        LightSample lightSample;
        lightSample.luminance = light.intensity / dot(siToLight, siToLight);
        lightSample.p = light.position;
        lightSample.wi = normalize(light.position - si.p);
        lightSample.pdf = 1.0f;
        return lightSample;
    } else {
        return dummyLightSample();
    }
}

#if SHADOW_TYPE == 0
bool light_testShadowMapVisibility(const SpotLight light, const SurfaceInteraction si, const LightSample lightSample)
{
    const float4 homogenousLightSpacePos = mul(light.vpMatrix, float4(si.p, 1.0));
    const float3 lightSpacePos = homogenousLightSpacePos.xyz / homogenousLightSpacePos.w;
    float2 shadowMapCoord = 0.5f * lightSpacePos.xy + 0.5f;
    shadowMapCoord.y = 1.0 - shadowMapCoord.y;
    const float shadowMapDepth = shadowMap.Sample(sampler1, shadowMapCoord).r;
    const float actualDepth = lightSpacePos.z;

    const float bias = 0.0001f;
    return actualDepth < shadowMapDepth + bias;
}
#endif

#endif // __SPOTLIGHT_HLSL__
