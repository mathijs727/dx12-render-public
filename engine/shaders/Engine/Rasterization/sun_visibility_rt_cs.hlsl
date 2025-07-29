#include "Engine/Lights/directional_light.hlsl"
#include "Engine/Util/random.hlsl"
#include "ShaderInputs/inputgroups/ComputeLayout/SunVisibilityRT.hlsl"

[numthreads(8, 8, 1)] void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    SunVisibilityRT inputs = g_sunVisibilityRT;

    const uint2 resolution = inputs.getNumThreads();
    if (dispatchThreadID.x >= resolution.x || dispatchThreadID.y >= resolution.y)
        return;
    
    const float3 surfacePosition = inputs.getPosition_metallic().Load(int3(dispatchThreadID.xy, 0)).xyz;
    DirectionalLight sun = inputs.getSun();
    LightSample lightSample = light_sample(sun);

    RayDesc ray;
    ray.Direction = lightSample.wi;
    ray.Origin = surfacePosition + 0.01f * ray.Direction;
    ray.TMin = 0;
    ray.TMax = 100000;

    // Instantiate ray query object. Template parameter allows driver to generate a specialized implementation.
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;

    // Set up a trace.  No work is done yet.
    q.TraceRayInline(inputs.getAccelerationStructure(), RAY_FLAG_NONE, 0xFF, ray);

    // Proceed() below is where behind-the-scenes traversal happens, including the heaviest of any driver inlined code.
    // In this simplest of scenarios, Proceed() only needs to be called once rather than a loop: based on the template
    // specialization above, traversal completion is guaranteed.
    q.Proceed();

    // Examine and act on the result of the traversal. Was a hit committed?
    const bool hit = q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;

    RWTexture2D<float> output = inputs.getOutput();
    output[dispatchThreadID.xy] = hit ? 0.0f : 1.0f;
    //output[dispatchThreadID.xy] = surfacePosition.x;
}
