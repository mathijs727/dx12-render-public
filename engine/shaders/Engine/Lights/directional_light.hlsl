#ifndef __DIRECTIONALLIGHT_HLSL__
#define __DIRECTIONALLIGHT_HLSL__
#include "Engine/Shared/surface_interaction.hlsl"
#include "Engine/Shared/constants.hlsl"
#include "ShaderInputs/structs/DirectionalLight.hlsl"
#include "light.hlsl"

LightSample light_sample(in const DirectionalLight light)
{
    LightSample lightSample;
    lightSample.luminance = light.intensity;
    lightSample.wi = -light.direction;
    lightSample.pdf = 1.0f;
    return lightSample;
}


LightSample light_sample(in const DirectionalLight light, in const SurfaceInteraction si, out RayDesc visibilityRay)
{
    LightSample lightSample = light_sample(light);
    visibilityRay.Origin = si.p + RAY_EPSILON * si.gn;
    visibilityRay.TMin = 0.0f;
    visibilityRay.Direction = lightSample.wi;
    visibilityRay.TMax = 1000000.0f;
    return lightSample;
}

#endif // __DIRECTIONALLIGHT_HLSL__
