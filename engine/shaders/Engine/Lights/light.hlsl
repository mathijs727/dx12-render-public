#ifndef __LIGHT_HLSL__
#define __LIGHT_HLSL__

struct LightSample {
    float3 luminance;
    float3 wi;
    float pdf;
};

LightSample dummyLightSample() {
    LightSample ret;
    ret.luminance = float3(0.0f, 0.0f, 0.0f);
    ret.wi = float3(0.0f, 0.0f, 0.0f);
    ret.pdf = 0.0f;
    return ret;
}

bool isDummyLightSample(const LightSample lightSample) {
    return lightSample.pdf == 0.0f;
}

#endif // __LIGHT_HLSL__
