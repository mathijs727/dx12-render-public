#ifndef __QUADLIGHT_HLSL__
#define __QUADLIGHT_HLSL__
#include "inputs/structs.hlsl"
#include "light.hlsl"
#include "shared/constants.hlsl"
#include "shared/surface_interaction.hlsl"
#include "util/random.hlsl"

float3 sampleQuadLight(QuadLight light, float2 u, out float pdf)
{
    // https://github.com/mmp/pbrt-v3/blob/master/src/shapes/triangle.cpp
    pdf = 1.0f / light.area;
    u = 2.0f * u - 1.0f;
    return light.position + u.x * light.u + u.y * light.v;
}

LightSample light_sample(const QuadLight light, const SurfaceInteraction si, const float2 u)
{
    float pdf = 0.0f;
    const float3 samplePos = sampleQuadLight(light, u, pdf);
    const float3 siToLight = samplePos - si.p;
    const float3 wi = normalize(siToLight);
    if (dot(-siToLight, light.n) <= 0.0f || dot(siToLight, si.n) <= 0.0f)
        return dummyLightSample();

    // https://github.com/mmp/pbrt-v3/blob/master/src/core/shape.cpp
    const float distanceSquared = dot(siToLight, siToLight);
    pdf *= distanceSquared / abs(dot(light.n, -wi));

    LightSample lightSample;
    lightSample.p = samplePos;
    lightSample.wi = wi;
    lightSample.pdf = pdf;
    // https://github.com/mmp/pbrt-v3/blob/master/src/lights/diffuse.h
    lightSample.luminance = light.luminance;
    return lightSample;
}

#endif // __QUADLIGHT_HLSL__
