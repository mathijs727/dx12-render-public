#include "Engine/Util/random.hlsl"
#include "ShaderInputs/inputgroups/ComputeLayout/RayTraceDebug.hlsl"
#include "Engine/RayTracing/rt_camera.hlsl"

[numthreads(8, 8, 1)] void main(uint3 dispatchThreadID
                                : SV_DispatchThreadID) {
    // Remap launch index to the range of [-1, +1)
    const float2 d = ((dispatchThreadID.xy + 0.5f) / g_rayTraceDebug.getNumThreads()) * 2.0f - 1.0f;

    RayDesc ray;
    generateRay(g_rayTraceDebug.getCamera(), d, ray);
    ray.TMin = 0;
    ray.TMax = 100000;

    // Instantiate ray query object. Template parameter allows driver to generate a specialized implementation.
    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;

    // Set up a trace.  No work is done yet.
    q.TraceRayInline(g_rayTraceDebug.getAccelerationStructure(), RAY_FLAG_NONE, 0xFF, ray);

    // Proceed() below is where behind-the-scenes traversal happens, including the heaviest of any driver inlined code.
    // In this simplest of scenarios, Proceed() only needs to be called once rather than a loop: based on the template
    // specialization above, traversal completion is guaranteed.
    q.Proceed();

    // Examine and act on the result of the traversal. Was a hit committed?
    float3 color = float3(0, 0, 0);
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        const uint stream = hash(q.CommittedPrimitiveIndex());
        RNG rng = createRandomNumberGenerator(0, stream);
        color = rng.generateFloat3();
    }

    RWTexture2D<float4> output = g_rayTraceDebug.getOutput();
    output[dispatchThreadID.xy] = float4(color, 1.0f);
}