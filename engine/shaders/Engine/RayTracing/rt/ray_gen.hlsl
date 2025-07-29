#include "common.hlsl"
#include "ShaderInputs/inputgroups/RayTraceGlobalLayout/RayTracePipelineDebug.hlsl"

RaytracingAccelerationStructure sceneBVH : register(t1);

[shader("raygeneration")] void rayGen() {
    const RayTracePipelineDebug inputs = g_rayTracePipelineDebug;

    const uint2 launchIndex = DispatchRaysIndex().xy;
    const float2 dims = float2(DispatchRaysDimensions().xy);
    // Remap launch index to the range of [-1, +1)
    const float2 d = ((launchIndex.xy + 0.5f) / dims.xy) * 2.0f - 1.0f;

    // Initialize the ray payload.
    HitInfo payload;
    payload.colorAndDistance = float4(0, 1, 0, 0);

    RayDesc ray;
    ray.Origin = inputs.getOrigin();
    ray.Direction = normalize(inputs.getForward() + d.x * inputs.getScreenU() + -d.y * inputs.getScreenV());
    ray.TMin = 0;
    ray.TMax = 100000;
    TraceRay(
        inputs.getAccelerationStructure(),
        RAY_FLAG_NONE,
        0xFF, // InstanceInclusionMask
        0, // RayContributionToHitGroupIndex (ray id)
        1, // MultiplierForGeometryContributionToHitGroupIndex
        0, // Miss shader index
        ray,
        payload);

    RWTexture2D<float4> output = inputs.getOutput();
    output[launchIndex] = float4(payload.colorAndDistance.rgb, 1.0f);
}