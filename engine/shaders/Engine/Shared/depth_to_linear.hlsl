#ifndef __DEPTH_TO_LINEAR_HLSL__
#define __DEPTH_TO_LINEAR_HLSL__

float depthToZ(float d, float n, float f)
{
    return (n * f) / ((f - n) * d - f);
}

float depthToLinear(float d, float n, float f)
{
    const float z = depthToZ(d, n, f);
    return (z + n) / (n - f);
}

#endif // __DEPTH_TO_LINEAR_HLSL__
