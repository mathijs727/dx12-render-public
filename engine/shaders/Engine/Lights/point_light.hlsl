#ifndef __POINTLIGHT_HLSL__
#define __POINTLIGHT_HLSL__
//#include "ShaderInputs/structs/PointLight.hlsl"
#include "light.hlsl"
#include "Engine/Shared/surface_interaction.hlsl"
#include "Engine/Util/random.hlsl"

/*// Copied from "Course notes on moving Frostbite to Physically Based Rendering":
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
const float smoothDistanceAtt(float squaredDistance, float invSqrAttRadius)
{
    const float factor = squaredDistance * invSqrAttRadius;
    const float smoothFactor = saturate(1.0f - factor * factor);
    return smoothFactor * smoothFactor;
}

LightSample light_sample(const PointLight light, const SurfaceInteraction si, const float2 u)
{
    const float3 siToLight = light.position - si.p;

    // See "Course notes on moving Frostbite to Physically Based Rendering":
    // https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
    const float3 I = light.luminousIntensity;
    const float distance2 = dot(siToLight, siToLight);

    static const float radius = 0.01f; // Assume point light has a 1cm radius
    static const float radius2 = radius * radius;
    const float3 E = I / max(distance2, radius2);

    LightSample lightSample;
    lightSample.luminance = E * smoothDistanceAtt(distance2, light.invSqrAttRadius);
    lightSample.p = light.position;
    lightSample.wi = normalize(light.position - si.p);
    lightSample.pdf = 1.0f;
    return lightSample;
}

#if SHADOW_TYPE == 0
bool light_testShadowMapVisibility(const PointLight light, const SurfaceInteraction si, const LightSample lightSample)
{
    return true;
}
#endif*/

#endif // __POINTLIGHT_HLSL__
