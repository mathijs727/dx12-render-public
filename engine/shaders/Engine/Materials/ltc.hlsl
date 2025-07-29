#ifndef __LTC_HLSL__
#define __LTC_HLSL__
#include "Engine/Shared/constants.hlsl"
#include "Engine/Shared/surface_interaction.hlsl"
#include "ShaderInputs/Structs/QuadLight.hlsl"

// Based on GLSL code from:
// https://github.com/selfshadow/ltc_code
// https://blog.selfshadow.com/sandbox/ltc.html
float integrateEdge(float3 v1, float3 v2)
{
    float cosTheta = dot(v1, v2);
    float theta = acos(cosTheta);
    return cross(v1, v2).z * ((theta > 0.001) ? theta / sin(theta) : 1.0) / (2.0 * PI);
}

void clipQuadToHorizon(inout float3 L[5], out int n);

// Based on GLSL code from:
// https://github.com/selfshadow/ltc_code
float3 LTC_Evaluate(in const SurfaceInteraction si, float3x3 invM, in const QuadLight quadLight)
{
    // Construct orhonormal basis around N.
    const float3 T1 = normalize(si.wo - si.n * dot(si.wo, si.n));
    const float3 T2 = cross(si.n, T1);

    // Rotate area light in (T1, T2, N) basis.
    invM = mul(invM, float3x3(T1, T2, si.n));

    // Polygon (allocate 5 vertices for clipping).
    float3 L[5];
    L[0] = mul(invM, (quadLight.position - quadLight.u - quadLight.v) - si.p);
    L[1] = mul(invM, (quadLight.position + quadLight.u - quadLight.v) - si.p);
    L[2] = mul(invM, (quadLight.position + quadLight.u + quadLight.v) - si.p);
    L[3] = mul(invM, (quadLight.position - quadLight.u + quadLight.v) - si.p);

    // Integrate.
    float sum = 0.0f;

    int n;
    clipQuadToHorizon(L, n);
    if (n == 0) // Completely over the horizon.
        return float3(0, 0, 0);

    // Project onto sphere.
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);
    L[4] = normalize(L[4]);

    // Integrate.
    sum += integrateEdge(L[0], L[1]);
    sum += integrateEdge(L[1], L[2]);
    sum += integrateEdge(L[2], L[3]);
    if (n >= 4)
        sum += integrateEdge(L[3], L[4]);
    if (n == 5)
        sum += integrateEdge(L[4], L[0]);
    sum = max(0.0, sum);

    return float3(sum, sum, sum);
}

void clipQuadToHorizon(inout float3 L[5], out int n)
{
    // detect clipping config
    int config = 0;
    if (L[0].z > 0.0)
        config += 1;
    if (L[1].z > 0.0)
        config += 2;
    if (L[2].z > 0.0)
        config += 4;
    if (L[3].z > 0.0)
        config += 8;

    // clip
    n = 0;

    if (config == 0) {
        // clip all
    } else if (config == 1) // V1 clip V2 V3 V4
    {
        n = 3;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[3].z * L[0] + L[0].z * L[3];
    } else if (config == 2) // V2 clip V1 V3 V4
    {
        n = 3;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    } else if (config == 3) // V1 V2 clip V3 V4
    {
        n = 4;
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
        L[3] = -L[3].z * L[0] + L[0].z * L[3];
    } else if (config == 4) // V3 clip V1 V2 V4
    {
        n = 3;
        L[0] = -L[3].z * L[2] + L[2].z * L[3];
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
    } else if (config == 5) // V1 V3 clip V2 V4) impossible
    {
        n = 0;
    } else if (config == 6) // V2 V3 clip V1 V4
    {
        n = 4;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    } else if (config == 7) // V1 V2 V3 clip V4
    {
        n = 5;
        L[4] = -L[3].z * L[0] + L[0].z * L[3];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    } else if (config == 8) // V4 clip V1 V2 V3
    {
        n = 3;
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
        L[1] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = L[3];
    } else if (config == 9) // V1 V4 clip V2 V3
    {
        n = 4;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[2].z * L[3] + L[3].z * L[2];
    } else if (config == 10) // V2 V4 clip V1 V3) impossible
    {
        n = 0;
    } else if (config == 11) // V1 V2 V4 clip V3
    {
        n = 5;
        L[4] = L[3];
        L[3] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    } else if (config == 12) // V3 V4 clip V1 V2
    {
        n = 4;
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
    } else if (config == 13) // V1 V3 V4 clip V2
    {
        n = 5;
        L[4] = L[3];
        L[3] = L[2];
        L[2] = -L[1].z * L[2] + L[2].z * L[1];
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
    } else if (config == 14) // V2 V3 V4 clip V1
    {
        n = 5;
        L[4] = -L[0].z * L[3] + L[3].z * L[0];
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
    } else if (config == 15) // V1 V2 V3 V4
    {
        n = 4;
    }

    if (n == 3)
        L[3] = L[0];
    if (n == 4)
        L[4] = L[0];
}

#endif // __LTC_HLSL__